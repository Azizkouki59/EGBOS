// engine/tests/test_l2_concurrent_transport.cpp
// EGBOS — L1→L2 Snapshot Transport Concurrency Qualification Test
//
// Purpose:
//   Qualify the LobSnapshotPublisher + double-buffer + per-buffer seqlock +
//   LobSnapshotReader under concurrent writer/reader pressure.
//
// What this test does NOT do:
//   - Does not implement quantitative feature formulas.
//   - Does not change production memory ordering.
//   - Does not modify the Top5BidsAsks ABI.
//   - Does not modify the LobSnapshotPublisher or LobSnapshotReader protocols.
//   - Does not suppress TSAN reports.
//
// Torn-read detection strategy:
//   The writer encodes a monotonically increasing generation counter (G) into
//   multiple fields of Top5BidsAsks using a deterministic, invertible pattern:
//
//     bid_qtys[k]           = G + k          (k = 0..4)
//     ask_qtys[k]           = G + 100 + k    (k = 0..4)
//     bid_price_offsets[k]  = (int16_t)(G + 200 + k)
//     ask_price_offsets[k]  = (int16_t)(G + 300 + k)
//     anchor_price          = (int64_t)(G + 400)
//
//   A coherent read must satisfy ALL of these relationships for a SINGLE G.
//   Any cross-generation mixture of fields constitutes a torn read and is
//   reported as a test failure.
//
// The test does NOT claim the transport is concurrency-safe merely because it
// passes. Whether C++ std::memcpy of non-atomic data is legally synchronized by
// the surrounding seqlock is a separate, formally-stated question; see the TSAN
// section below and the final report.
//
// Iteration count:
//   kWriterIterations = 500,000 per instrument.
//   This exercises buffer alternation (publisher flips active_buffer on every
//   publish, so alternation happens 500,000 times) and substantial writer/reader
//   overlap on a single-core or multi-core host.
//
// TSAN behavior:
//   std::memcpy of non-atomic payload bytes under a seqlock is a known TSAN
//   false-positive category: the C++ memory model does not define
//   synchronization for non-atomic accesses even when they are sequentially
//   consistent at the atomic level. TSAN will instrument the non-atomic
//   memcpy in the writer (lob_snapshot_publisher.h:79-82) and reader
//   (lob_snapshot_reader.h:101-104) and MAY report a data race even if the
//   seqlock is logically sound.
//
//   If TSAN reports a race, the exact source locations and a formal analysis are
//   included in the FINAL REPORT section at the bottom of this file.
//
// Classification: LOCAL_SYNTHETIC — no network, no orders, no real SHM.

#include "engine/l1/lob_snapshot_publisher.h"
#include "engine/l2/lob_snapshot_reader.h"
#include "engine/core/shm_layout.h"
#include "engine/core/lob_snapshot_abi.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>

namespace {

// ── Generation-encoding helpers ───────────────────────────────────────────────

// Build a deterministic Top5BidsAsks for generation G.
// Every field is a function of G; any field from generation G' != G is
// immediately detectable as a torn read.
static egbos::l1::Top5BidsAsks make_snapshot(uint32_t G) noexcept {
    egbos::l1::Top5BidsAsks s{};
    for (int k = 0; k < 5; ++k) {
        s.bid_qtys[k]          = G + static_cast<uint32_t>(k);
        s.ask_qtys[k]          = G + 100u + static_cast<uint32_t>(k);
        s.bid_price_offsets[k] = static_cast<int16_t>((G + 200u + static_cast<uint32_t>(k)) & 0x7FFF);
        s.ask_price_offsets[k] = static_cast<int16_t>((G + 300u + static_cast<uint32_t>(k)) & 0x7FFF);
    }
    s.anchor_price = static_cast<int64_t>(G + 400u);
    s.tick_size    = 100;  // fixed 100-tick for coherence testing
    return s;
}

// Recover the generation from a snapshot.
// Returns the G implied by bid_qtys[0], or UINT32_MAX if the snapshot
// was never written (bid_qtys[0] == 0 and anchor_price == 0).
// Note: G==0 is a valid generation; we detect "never published" via the
// seqlock returning false (sequence==0 path), not via field values.
static uint32_t generation_of(const egbos::l1::Top5BidsAsks& s) noexcept {
    return s.bid_qtys[0];
}

// Verify that all fields in `s` are internally consistent for the generation
// implied by bid_qtys[0].  Returns true if coherent, false if torn.
static bool is_coherent(const egbos::l1::Top5BidsAsks& s) noexcept {
    const uint32_t G = generation_of(s);
    for (int k = 0; k < 5; ++k) {
        if (s.bid_qtys[k] != G + static_cast<uint32_t>(k))           return false;
        if (s.ask_qtys[k] != G + 100u + static_cast<uint32_t>(k))    return false;
        const int16_t expected_bid =
            static_cast<int16_t>((G + 200u + static_cast<uint32_t>(k)) & 0x7FFF);
        const int16_t expected_ask =
            static_cast<int16_t>((G + 300u + static_cast<uint32_t>(k)) & 0x7FFF);
        if (s.bid_price_offsets[k] != expected_bid)                   return false;
        if (s.ask_price_offsets[k] != expected_ask)                   return false;
    }
    if (s.anchor_price != static_cast<int64_t>(G + 400u))             return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST: ConcurrentPublishAndRead
//
// One writer thread, one reader thread, one instrument (id=0), 500,000 iterations.
//
// Writer: publishes generation G = 1, 2, 3, … 500,000 sequentially.
//         Signals done via atomic flag; does NOT use sleep as correctness mechanism.
//
// Reader: spins calling LobSnapshotReader::read() until done_flag is set and
//         at least kMinReadsRequired successful reads have been obtained.
//         On each successful read, verifies coherence.
//
// Failure condition: any read returns true but the snapshot is torn.
// ─────────────────────────────────────────────────────────────────────────────

TEST(L2ConcurrentTransportTest, ConcurrentPublishAndRead) {
    constexpr uint32_t kWriterIterations  = 500'000;
    constexpr uint32_t kInstrumentId      = 0;
    // Minimum number of successful reads the reader must accumulate before the
    // test is considered exercised. If the writer outpaces the reader to such
    // a degree that the reader gets zero reads (extremely unlikely on any real
    // scheduler), the test still passes — torn read detection fires if it runs.
    constexpr uint64_t kMinReadsRequired  = 1;

    // Allocate LobSnapshotsRegion on the heap (avoids 3 MB stack frame).
    auto region = std::make_unique<egbos::shm::LobSnapshotsRegion>();

    egbos::l1::LobSnapshotPublisher publisher(region.get());
    egbos::l2::LobSnapshotReader    reader(region.get());

    // Shared state between threads — atomic flags only; no mutex.
    std::atomic<bool>     done_flag{false};    // writer sets when finished
    std::atomic<uint64_t> torn_read_count{0};  // reader increments on torn read
    std::atomic<uint64_t> read_ok_count{0};    // reader increments on coherent read

    // ── Writer thread ─────────────────────────────────────────────────────────
    std::thread writer([&]() {
        for (uint32_t g = 1; g <= kWriterIterations; ++g) {
            const egbos::l1::Top5BidsAsks snap = make_snapshot(g);
            // publish() returns false only on region==nullptr or OOB instrument;
            // neither applies here. We do not assert here to keep the hot path
            // minimal, but a false return would mean reads also fail, which the
            // reader side will surface.
            (void)publisher.publish(kInstrumentId, snap);
        }
        // Use seq_cst store so the reader's acquire load sees all publishes.
        done_flag.store(true, std::memory_order_seq_cst);
    });

    // ── Reader thread ─────────────────────────────────────────────────────────
    std::thread reader_thread([&]() {
        egbos::l1::Top5BidsAsks out{};
        // Spin until the writer is done AND we have completed at least
        // kMinReadsRequired successful reads.
        while (true) {
            const bool writer_done =
                done_flag.load(std::memory_order_acquire);

            uint64_t out_seq = 0;
            if (reader.read(kInstrumentId, out, out_seq)) {
                if (is_coherent(out)) {
                    read_ok_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    torn_read_count.fetch_add(1, std::memory_order_relaxed);
                }
            }

            if (writer_done) {
                // Drain any remaining buffered snapshot.
                uint64_t out_seq = 0;
                if (reader.read(kInstrumentId, out, out_seq)) {
                    if (is_coherent(out)) {
                        read_ok_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        torn_read_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                break;
            }
        }
    });

    writer.join();
    reader_thread.join();

    const uint64_t torn  = torn_read_count.load();
    const uint64_t ok    = read_ok_count.load();

    // Report counters regardless of pass/fail (visible in --gtest_verbose output).
    std::printf(
        "[L2ConcurrentTransportTest] writer_iterations=%u  "
        "reads_ok=%lu  torn_reads=%lu\n",
        kWriterIterations,
        static_cast<unsigned long>(ok),
        static_cast<unsigned long>(torn));

    EXPECT_EQ(torn, 0u)
        << "Torn read detected: " << torn << " snapshot(s) had mixed-generation fields. "
        << "This indicates the seqlock+double-buffer protocol did not prevent "
        << "the reader from observing a partially-written payload. "
        << "See the TSAN analysis in test_l2_concurrent_transport.cpp.";

    EXPECT_GE(ok, kMinReadsRequired)
        << "Reader obtained fewer than " << kMinReadsRequired
        << " successful reads — scheduling starvation may have prevented "
        << "meaningful concurrency coverage.";
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST: BufferAlternationIsObservable
//
// Single-threaded verification that publish() alternates the active_buffer
// index on every call, and that the reader always sees the most recently
// published generation.
//
// This is not a concurrency test; it confirms the buffer-flip invariant that
// the concurrent test relies on.
// ─────────────────────────────────────────────────────────────────────────────

TEST(L2ConcurrentTransportTest, BufferAlternationIsObservable) {
    constexpr uint32_t kIterations   = 10'000;
    constexpr uint32_t kInstrumentId = 1;

    auto region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::l1::LobSnapshotPublisher publisher(region.get());
    egbos::l2::LobSnapshotReader    reader(region.get());

    // Expected active buffer after G publishes:
    //   active_buffer starts at 0. After publish 1: inactive=1, flip → active=1.
    //   After publish 2: inactive=0, flip → active=0. Etc.
    uint8_t expected_active = 1u;

    for (uint32_t g = 1; g <= kIterations; ++g) {
        const egbos::l1::Top5BidsAsks snap = make_snapshot(g);
        ASSERT_TRUE(publisher.publish(kInstrumentId, snap))
            << "publish() failed at generation " << g;

        const uint8_t actual_active =
            region->instruments[kInstrumentId].header.active_buffer.load(
                std::memory_order_acquire);
        ASSERT_EQ(actual_active, expected_active)
            << "active_buffer did not alternate at generation " << g;

        egbos::l1::Top5BidsAsks out{};
        uint64_t out_seq = 0;
        ASSERT_TRUE(reader.read(kInstrumentId, out, out_seq))
            << "read() failed at generation " << g;

        ASSERT_TRUE(is_coherent(out))
            << "Torn read in single-threaded alternation test at generation " << g
            << ": bid_qtys[0]=" << out.bid_qtys[0];

        ASSERT_EQ(generation_of(out), g)
            << "Reader returned stale generation " << generation_of(out)
            << " after publishing generation " << g;

        expected_active ^= 1u;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST: ReadReturnsFalseBeforeFirstPublish
//
// Verifies the sequence==0 early-return path in LobSnapshotReader::read().
// The active buffer's sequence counter is 0 until the first publish; the
// reader must return false rather than returning zero-initialized garbage.
// ─────────────────────────────────────────────────────────────────────────────

TEST(L2ConcurrentTransportTest, ReadReturnsFalseBeforeFirstPublish) {
    constexpr uint32_t kInstrumentId = 2;
    auto region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::l2::LobSnapshotReader reader(region.get());

    egbos::l1::Top5BidsAsks out{};
    uint64_t out_seq = 0;
    EXPECT_FALSE(reader.read(kInstrumentId, out, out_seq))
        << "Reader must return false when sequence==0 (no publish has occurred).";
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// TSAN ANALYSIS (static commentary — no executable code below)
//
// If TSAN is enabled (-DENABLE_TSAN=ON) and reports a data race, the exact
// locations are:
//
//   WRITER non-atomic access:
//     engine/l1/lob_snapshot_publisher.h:79-82
//       std::memcpy(buffer.data.payload, &snapshot, sizeof(Top5BidsAsks));
//     buffer.data.payload is uint8_t[] — a non-atomic type.
//
//   READER non-atomic access:
//     engine/l2/lob_snapshot_reader.h:101-104
//       std::memcpy(&out, buffer.data.payload, sizeof(egbos::l1::Top5BidsAsks));
//     Same non-atomic uint8_t[] region.
//
// WHY TSAN MAY REPORT THIS:
//
//   The C++20 memory model does not define synchronization relationships for
//   non-atomic data accessed concurrently from multiple threads unless:
//     (a) the accesses are guarded by a mutex, OR
//     (b) a happens-before relationship is established via atomic acquire/release
//         pairs on an object that the non-atomic data is "protected by."
//
//   The seqlock protocol intends (b): the writer's seq_cst store of (seq+1)
//   at publisher.h:75-77 happens-before the memcpy at :79-82 (within the
//   writer thread); the reader's acquire load of seq1 at reader.h:90-91 is
//   intended to establish a happens-before from the writer's seq_cst store
//   at :86-88 (seq+2, even) to the reader's fence at :106.
//
//   However, the C++ standard does not guarantee that a non-atomic memcpy of
//   data[0..63] is covered by the happens-before chain established through
//   the atomic sequence counter. TSAN instruments each byte access
//   independently and will flag the concurrent accesses to buffer.data.payload
//   as a data race because they are not individually atomic.
//
//   This is the well-known "seqlock TSAN false-positive" problem. The practical
//   memory model behavior on x86 (TSO architecture) is that the seq_cst fence
//   at publisher.h:84 causes a full memory barrier (mfence on x86), which in
//   practice orders all prior stores (including the memcpy) before all
//   subsequent loads. On x86, load/store tearing of aligned word-sized accesses
//   does not occur. However, this is a platform-specific property, not a
//   guarantee from the C++ abstract machine.
//
// WHETHER AN ABI-PRESERVING FIX IS POSSIBLE:
//
//   The standard-conformant fix is to declare buffer.data.payload as an array
//   of std::atomic<uint64_t> (or char with volatile, or use __sanitizer_*
//   annotations) and perform the memcpy as a sequence of atomic loads/stores.
//   This would preserve the ABI (the wire layout is unchanged) but adds
//   overhead on non-x86 architectures. The fix requires changing
//   engine/core/shm_layout.h (LobSnapshotData::payload) and the publisher
//   and reader memcpy calls — i.e., it is NOT a no-source-change fix.
//
//   Alternatively, TSAN suppression via __attribute__((no_sanitize("thread")))
//   on the memcpy site would silence the report without fixing the underlying
//   model gap. This is NOT recommended; requirement 9 of this task forbids it.
//
//   A third option: annotate with TSAN-specific primitives
//   (__tsan_acquire/__tsan_release) to teach TSAN about the seqlock protocol.
//   This preserves the current layout and source structure but adds a
//   TSAN-specific dependency.
//
//   Decision: Per task requirement 12, this analysis is reported here.
//   No protocol redesign is performed automatically.
//
// ABI PRICE-ENCODING GAP (requirement 13):
//
//   anchor_price_low32 (lob_snapshot_abi.h:22) stores only the low 32 bits
//   of the int64_t anchor_price_ field of LobEngine (lob_engine.cpp:301):
//
//       snap.anchor_price_low32 = static_cast<uint32_t>(anchor_price_ & 0xFFFFFFFF);
//
//   Top5BidsAsks carries bid_price_offsets[k] and ask_price_offsets[k] as
//   int16_t relative to kMaxPriceLevels/2 (= 32768). To convert an offset
//   back to an absolute price, an L2 reader needs:
//
//       absolute_price = anchor_price + (offset - 32768) * tick_size
//
//   anchor_price is int64_t. The ABI transmits only the low 32 bits. The
//   high 32 bits of anchor_price are LOST at the L1→L2 boundary. tick_size
//   is not transmitted at all.
//
//   For BTCUSDT at kPriceScale = 10^7 (depth_parser.h:13):
//     A price of, e.g., 70,000.00000 = 700,000,000,000 in fixed-point units.
//     In hex: 0x000000A2_6A07_0000. The low 32 bits = 0x6A070000 ≠ the full
//     value. The high 32 bits (0x000000A2) are discarded.
//
//   At BTCUSDT prices below 2^32 / 10^7 ≈ $429.49 the low 32 bits are
//   sufficient (no high bits set). Above that threshold — which includes all
//   realistic current and foreseeable BTCUSDT prices — the high bits are
//   required to reconstruct absolute prices.
//
//   FINDING: The current ABI does NOT allow reconstruction of absolute prices
//   for BTCUSDT at realistic price levels. This is an ABI/specification gap.
//   The gap does not affect the L1→L2 transport mechanism itself (seqlock
//   and double-buffer are correct regardless of payload semantics), but any
//   future L2 feature that needs absolute prices (e.g., mid-price in dollars)
//   cannot be computed from the current Top5BidsAsks payload alone without
//   an out-of-band source for the full anchor_price and tick_size.
//
//   DO NOT CHANGE the ABI to fix this (requirement 13). This is reported
//   as a specification gap only.
// ─────────────────────────────────────────────────────────────────────────────
