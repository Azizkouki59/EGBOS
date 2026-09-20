// scripts/testnet_diagnostic_runner.cpp
// EGBOS — Testnet Diagnostic Runner
//
// Constructs a real BinanceFeedListener against the Binance TESTNET endpoint,
// runs it on a background thread, polls MarketEventsRegion for 30 seconds,
// and for each popped MarketEventSlot writes a WalRecord to exercise the
// real C++ receive → ingest → WAL path end-to-end.
//
// Provenance : DEVELOPMENT_TEST — NEVER LIVE_OPERATIONAL
// Endpoint   : wss://stream.testnet.binance.vision:9443/ws/btcusdt@depth
// Governance : MARKET-DATA-001 §D9 — testnet data must not be promoted
//              to LIVE_OPERATIONAL evidence.
//
// WAL field semantics for records written by this runner:
//   record_type    = REGIME_CHANGE (no ORDER_NEW: this is a market-depth stream)
//   instrument_id  = 0  (no instrument mapping in the ingestor stub; do not fabricate)
//   order_id       = diagnostic counter only — NOT an exchange order ID
//   quantity_delta = 0  (FeedIngestor stub does not parse JSON; do not fabricate)
//   price          = 0  (FeedIngestor stub does not parse JSON; do not fabricate)
//   payload        = raw 128-byte MarketEventSlot::data from the SPSC ring
//                    (contains raw Binance JSON bytes truncated to 120 bytes,
//                     with rx_timestamp_ns injected at offset 24 by the ingestor)
//
// Constraints:
//   - TESTNET endpoint only
//   - DEVELOPMENT_TEST provenance enforced at runtime
//   - Not a qualification run; does not touch governance/

#include "engine/l0/binance_feed_listener.h"
#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"
#include "engine/core/wal_writer.h"
#include "engine/l1/l1_consumer.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>

using namespace egbos::l0;
using namespace egbos::wal;
using namespace egbos::shm;

int main() {
#if defined(__linux__)
    ::signal(SIGPIPE, SIG_IGN);
#endif

    std::cout << "==================================================\n"
              << " EGBOS Testnet Diagnostic Runner\n"
              << " Provenance: DEVELOPMENT_TEST\n"
              << " Endpoint  : wss://testnet.binance.vision:443/ws\n"
              << "==================================================\n";

    // ── SHM regions ──
    static MarketEventsRegion s_market_events{};
    static GovernanceFlags    s_gov_flags{};

    // ── L1 Consumer ──
    egbos::l1::L1Consumer l1_consumer(s_market_events, s_gov_flags);

    // ── WalWriter ──
    // Open WAL in the current working directory.
    WalWriter wal("wal_primary.bin", "wal_shadow.bin");
    wal.start_session();

    // ── FeedIngestor ──
    // tier0_fd = -1 since we are manually reading from the ring and writing to WalWriter
    FeedIngestor ingestor(&s_market_events, &s_gov_flags, -1);

    // ── BinanceFeedListener ──
    BinanceFeedConfig config;
    config.use_testnet    = true;
    config.host           = "stream.testnet.binance.vision";
    config.port           = 9443;
    config.path           = "/ws/btcusdt@depth"; // High volume stream
    config.auto_reconnect = true;

    BinanceFeedListener listener(ingestor, config);
    std::cout << "Listener Provenance: " << listener.provenance_str() << "\n\n";

    if (listener.provenance() != FeedProvenance::DEVELOPMENT_TEST) {
        std::cerr << "ERROR: Runner must only run against TESTNET!\n";
        return 1;
    }

    std::thread listener_thread([&listener]() {
        listener.run_loop();
    });

    std::cout << "Starting 30-second capture to wal_primary.bin ...\n";

    auto start_time = std::chrono::steady_clock::now();
    uint64_t records_written = 0;

    alignas(8) uint8_t buf[kMaxWalRecordWireSize]{};
    auto* rec = reinterpret_cast<WalRecord*>(buf);

    // slot.data is 128 bytes; kMaxPayloadSize is 256 bytes — fits without truncation.
    // This assert fires at compile time if shm_layout.h or wal_writer.h change sizes.
    static_assert(sizeof(MarketEventSlot::data) <= kMaxPayloadSize,
        "MarketEventSlot::data exceeds WalRecord kMaxPayloadSize — payload would be truncated");

    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(30)) {
        MarketEventSlot slot;
        if (s_market_events.pop(slot)) {
            // L1 parser processing (in addition to WAL write)
            l1_consumer.process_slot(slot);

            // ── WAL record construction ────────────────────────────────────────
            // FABRICATION-REMOVAL PATCH (audit finding 1):
            // FeedIngestor::normalize_and_forward() is a stub that does NOT parse
            // Binance JSON. Real price and quantity do not exist as structured
            // numeric fields anywhere in this data pipeline. Setting them to 0
            // is honest; the previous 50000*kPriceScale / 100 were fabricated
            // values that could mislead any reader of this WAL.
            //
            // record_type: REGIME_CHANGE is the closest existing non-order type.
            //   ORDER_NEW was semantically wrong for a market-depth stream event.
            //   A purpose-specific WalRecordType would require an ADR amendment.
            //
            // instrument_id: 0 — no symbol→ID mapping exists; do not fabricate 1.
            //
            // order_id: diagnostic-local sequence counter only (SYNTHETIC, not an
            //   exchange order ID). Acceptable for WAL record identity.
            //
            // price / quantity_delta: 0 — parsed values unavailable from stub;
            //   zero is unambiguous; a plausible-looking fake would mislead.
            //
            // payload: full 128-byte slot.data — real ingestor output.
            //   Content: raw Binance JSON bytes (capped at 120 bytes, zero-padded),
            //   with rx_timestamp_ns written at offset 24 by the ingestor.
            //   Zero-padding beyond the actual JSON length is included.

            rec->record_type   = static_cast<uint32_t>(WalRecordType::REGIME_CHANGE);
            rec->instrument_id = 0;               // no instrument mapping — do not fabricate
            rec->order_id      = records_written; // diagnostic counter — NOT exchange order ID
            rec->quantity_delta = 0;              // not parsed; do not fabricate
            rec->price         = 0;               // not parsed; do not fabricate

            // payload_length = sizeof(slot.data) - 4 = 124 bytes.
            // The WAL writer computes CRC32C over payload[0..payload_length-5]
            // and stores it in payload[payload_length-4..payload_length-1].
            // Setting payload_length = 128 would cause the writer to overwrite
            // slot.data[124..127] with the CRC, corrupting the last 4 bytes of
            // the stored slot.  Using 124 reserves the last 4 bytes as the
            // trailer; bytes [0..119] carry the actual slot content.
            rec->payload_length = static_cast<uint32_t>(sizeof(slot.data)) - 4; // = 124
            std::memcpy(rec->payload, slot.data, rec->payload_length);

            wal.write_sync(rec);
            records_written++;
        } else {
            // Back off to avoid burning 100% CPU on an empty ring.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    std::cout << "Capture complete. Stopping listener...\n";
    listener.request_stop();
    listener_thread.join();

    std::cout << "Wrote " << records_written << " real WAL records to wal_primary.bin.\n";

    // Print the parse results
    l1_consumer.print_summary();

    return 0;
}
