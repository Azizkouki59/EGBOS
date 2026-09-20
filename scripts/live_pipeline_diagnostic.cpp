// scripts/live_pipeline_diagnostic.cpp
// End-to-end pipeline diagnostic: production → BinanceFeedListener →
// parse_depth_update → LobEngine → printed snapshot.
//
// Classification: LIVE_OPERATIONAL (production feed)
// Valid for AC-003 and AC-004 qualification evidence.
//
// Design: frame_arrival_cb fires on the listener thread immediately after
// process_payload, while frame_buf_/frame_len_ are still valid. We read the
// frame payload via the public frame_buf()/frame_len() accessors added for
// this purpose, bypassing the SHM/ring-buffer path entirely.

#include "engine/l0/binance_feed_listener.h"
#include "engine/l0/ingestor.h"
#include "engine/l1/depth_parser.h"
#include "engine/l1/lob_engine.h"

#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <string>

using namespace egbos::l0;
using namespace egbos::l1;

// ─── Shared state (listener thread → main thread) ──────────────────────────
static std::atomic<uint64_t> g_frames_received{0};
static std::atomic<uint64_t> g_parse_success{0};
static std::atomic<uint64_t> g_parse_failures{0};
static std::atomic<uint64_t> g_gaps_detected{0};

static LobEngine* g_lob        = nullptr;
static uint64_t   g_expected_seq = 0;
static bool       g_first_frame  = true;
int64_t g_diagnostic_anchor = 0;
static uint64_t   g_last_connect_count = 1;

// ─── Core frame handler ─────────────────────────────────────────────────────
static void on_frame_impl(const uint8_t* buf, std::size_t len, uint64_t connect_count) {
    g_frames_received.fetch_add(1, std::memory_order_relaxed);

    ParsedDepthUpdate upd;
    ParseResult r = parse_depth_update(buf, len, &upd);

    if (r == ParseResult::OK) {
        g_parse_success.fetch_add(1, std::memory_order_relaxed);

        if (connect_count > g_last_connect_count) {
            g_last_connect_count = connect_count;
            g_first_frame = true;
            delete g_lob;
            g_lob = nullptr;
            std::printf("RECONNECT BASELINE: Connect count %lu. Re-baselining sequence logic...\n", connect_count);
        }

        if (g_first_frame) {
            g_first_frame    = false;
            g_expected_seq   = upd.first_update_id;
        }
        if (!g_lob && upd.bid_count > 0) {
            // tick_size = kPriceScale / 100 = 100,000 fixed-point units.
            // Binance BTCUSDT minimum tick is $0.01; expressed in
            // kPriceScale (10^7) units: 10,000,000 * 0.01 = 100,000.
            // kPriceScale is defined in engine/l1/depth_parser.h.
            g_diagnostic_anchor = upd.bids[0].price;
            g_lob = new LobEngine(upd.bids[0].price, kPriceScale / 100);
        }

        if (upd.first_update_id != g_expected_seq) {
            g_gaps_detected.fetch_add(1, std::memory_order_relaxed);
            std::printf("GAP: expected U=%lu got U=%lu\n",
                        g_expected_seq, upd.first_update_id);
        }
        g_expected_seq = upd.final_update_id + 1;

        if (g_lob) {
            for (std::size_t i = 0; i < upd.bid_count; ++i)
                g_lob->upsert_synthetic_level(
                    upd.bids[i].price,
                    static_cast<uint32_t>(upd.bids[i].qty), true);
            for (std::size_t i = 0; i < upd.ask_count; ++i)
                g_lob->upsert_synthetic_level(
                    upd.asks[i].price,
                    static_cast<uint32_t>(upd.asks[i].qty), false);
            
            static Top5BidsAsks g_last_snap{};
            Top5BidsAsks snap = g_lob->generate_top5_snapshot();
            if (std::memcmp(&snap, &g_last_snap, sizeof(Top5BidsAsks)) != 0) {
                g_last_snap = snap;
                double anchor_dbl = g_diagnostic_anchor / static_cast<double>(kPriceScale);
                double tick_dbl = 100000.0 / kPriceScale; // $0.01

                std::printf("  Bids (Price/Qty): ");
                bool any_bid = false;
                for (int i = 0; i < 5; ++i)
                    if (snap.bid_qtys[i] > 0) {
                        double price = anchor_dbl + (snap.bid_price_offsets[i] * tick_dbl);
                        double qty = snap.bid_qtys[i] / static_cast<double>(kPriceScale);
                        std::printf("[%.2f / %.5f] ", price, qty);
                        any_bid = true;
                    }
                if (!any_bid) std::printf("(none)");
                
                std::printf("\n  Asks (Price/Qty): ");
                bool any_ask = false;
                for (int i = 0; i < 5; ++i)
                    if (snap.ask_qtys[i] > 0) {
                        double price = anchor_dbl + (snap.ask_price_offsets[i] * tick_dbl);
                        double qty = snap.ask_qtys[i] / static_cast<double>(kPriceScale);
                        std::printf("[%.2f / %.5f] ", price, qty);
                        any_ask = true;
                    }
                if (!any_ask) std::printf("(none)");
                std::printf("\n");
            }
        }
    } else {
        g_parse_failures.fetch_add(1, std::memory_order_relaxed);
        char hex[64]{};
        for (std::size_t i = 0; i < std::min(len, (std::size_t)16); ++i)
            std::snprintf(hex + i*3, sizeof(hex)-i*3, "%02x ", buf[i]);
        std::printf("PARSE_ERR code=%d len=%zu hex=%s\n",
                    static_cast<int>(r), len, hex);
    }
}

// ─── Main ───────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    int duration_s = 60;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--duration" && i + 1 < argc)
            duration_s = std::stoi(argv[i + 1]);

    std::printf("=== live_pipeline_diagnostic ===\n");
    std::printf("Endpoint : wss://stream.binance.com:9443/ws/btcusdt@depth\n");
    std::printf("Duration : %d s\n", duration_s);
    std::printf("Classification: LIVE_OPERATIONAL\n\n");
    std::fflush(stdout);

    // Real FeedIngestor with null SHM (tier-0 fd == -1).
    // process_payload is called by run_loop but writes nothing because fd==-1.
    FeedIngestor ingestor(nullptr, nullptr, -1);

    BinanceFeedConfig config;
    config.use_testnet    = false;
    config.host           = "stream.binance.com";
    config.port           = 9443;
    config.path           = "/ws/btcusdt@depth";
    config.auto_reconnect = true;

    BinanceFeedListener listener(ingestor, config);

    // Register frame-arrival callback.
    // Fired on the listener thread after process_payload, while frame_buf_
    // is still valid. We read the payload via the public accessors.
    listener.set_frame_arrival_callback([&listener](uint64_t) {
        on_frame_impl(listener.frame_buf(), listener.frame_len(), listener.connect_count());
    });

    std::thread t([&listener]{ listener.run_loop(); });

    auto t0       = std::chrono::steady_clock::now();
    auto last_snap = t0;

    // Run indefinitely if duration_s <= 0, otherwise stop after duration_s seconds.
    while (duration_s <= 0 || std::chrono::steady_clock::now() - t0 < std::chrono::seconds(duration_s)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now = std::chrono::steady_clock::now();

        if (now - last_snap >= std::chrono::seconds(5)) {
            last_snap = now;
            int elapsed = (int)std::chrono::duration_cast<
                std::chrono::seconds>(now - t0).count();
            std::printf("\n[%3ds] state=%d frames=%lu ok=%lu fail=%lu gaps=%lu\n",
                elapsed,
                static_cast<int>(listener.state()),
                g_frames_received.load(),
                g_parse_success.load(),
                g_parse_failures.load(),
                g_gaps_detected.load());

            if (!g_lob) {
                std::printf("  (awaiting first depth update...)\n");
            }
            std::fflush(stdout);
        }
    }

    listener.request_stop();
    t.join();

    std::printf("\n=== FINAL SUMMARY ===\n");
    std::printf("Frames received : %lu\n", g_frames_received.load());
    std::printf("Parse success   : %lu\n", g_parse_success.load());
    std::printf("Parse failures  : %lu\n", g_parse_failures.load());
    std::printf("Gaps detected   : %lu\n", g_gaps_detected.load());
    std::printf("Pipeline OK     : %s\n",
                g_parse_success.load() > 0 ? "YES" : "NO — no frames parsed");

    delete g_lob;
    return g_parse_success.load() > 0 ? 0 : 1;
}
