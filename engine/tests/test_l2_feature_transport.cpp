// engine/tests/test_l2_feature_transport.cpp
// EGBOS — L2 → L3 FeatureRegion ABI transport tests
//
// Covers:
//   T01  publish + read one instrument
//   T02  second publish replaces first safely
//   T03  unpublished slot rejected (seq == 0)
//   T04  odd-sequence (torn write) rejected
//   T05  schema version mismatch rejected
//   T06  source_seq preserved through round-trip
//   T07  two different instruments are independent (no cross-slot contamination)
//   T08  concurrent one-writer / one-reader correctness (10 000 iterations)
//   T09  instrument_id out of range rejected by FeaturePublisher and FeatureReader
//   T10  payload_bytes=0 + nullptr payload: reader sees zeroed payload

#include <gtest/gtest.h>
#include <atomic>
#include <cstring>
#include <thread>
#include <cstdint>

#include "core/shm_layout.h"
#include "l2/feature_publisher.h"
#include "l2/feature_reader.h"

using egbos::shm::FeatureRegion;
using egbos::shm::FeatureSlot;
using egbos::shm::kFeatureSlotPayloadBytes;
using egbos::shm::kMaxInstruments;
using egbos::l2::FeaturePublisher;
using egbos::l2::FeatureReader;
using egbos::l2::FeatureSlotView;
using egbos::l2::kFeatureSlotSchemaVersion;

// ── Helpers ──────────────────────────────────────────────────────────────────

static FeatureRegion* alloc_region() {
    // Use aligned_alloc so FeatureSlot's alignas(64) is satisfied.
    void* raw = std::aligned_alloc(64, sizeof(FeatureRegion));
    auto* region = new (raw) FeatureRegion{};
    return region;
}

static void free_region(FeatureRegion* r) {
    r->~FeatureRegion();
    std::free(r);
}

// Build a small distinguishable payload.
static void make_payload(uint8_t* buf, uint32_t bytes, uint8_t seed) {
    for (uint32_t i = 0; i < bytes; ++i) {
        buf[i] = static_cast<uint8_t>((seed + i) & 0xFF);
    }
}

// ── T01: publish + read one instrument ───────────────────────────────────────
TEST(FeatureTransportTest, T01_PublishReadOneInstrument) {
    FeatureRegion* region = alloc_region();
    FeaturePublisher writer(region);
    FeatureReader    reader(region);

    uint8_t payload[kFeatureSlotPayloadBytes];
    make_payload(payload, kFeatureSlotPayloadBytes, 42);

    const uint32_t iid       = 7u;
    const uint64_t src_seq   = 12345u;
    const uint64_t ts        = 999'000'000u;
    const uint64_t vmask     = 0x0000'0001u;
    const uint16_t fcount    = 1u;
    const uint32_t pbytes    = kFeatureSlotPayloadBytes;

    bool ok = writer.publish(iid, src_seq, ts, vmask, fcount, payload, pbytes);
    ASSERT_TRUE(ok) << "publish must succeed for valid instrument_id";

    FeatureSlotView view{};
    ok = reader.try_read(iid, view);
    ASSERT_TRUE(ok) << "try_read must succeed after publish";

    EXPECT_EQ(view.instrument_id,  iid);
    EXPECT_EQ(view.schema_version, kFeatureSlotSchemaVersion);
    EXPECT_EQ(view.source_seq,     src_seq);
    EXPECT_EQ(view.pub_ts_ns,      ts);
    EXPECT_EQ(view.validity_mask,  vmask);
    EXPECT_EQ(view.payload_count,  fcount);
    EXPECT_EQ(view.payload_bytes,  pbytes);
    EXPECT_EQ(0, std::memcmp(view.payload, payload, kFeatureSlotPayloadBytes))
        << "payload must survive round-trip intact";

    free_region(region);
}

// ── T02: second publish replaces first ───────────────────────────────────────
TEST(FeatureTransportTest, T02_SecondPublishReplacesFirst) {
    FeatureRegion* region = alloc_region();
    FeaturePublisher writer(region);
    FeatureReader    reader(region);

    const uint32_t iid = 0u;
    uint8_t pay1[kFeatureSlotPayloadBytes]; make_payload(pay1, kFeatureSlotPayloadBytes, 1);
    uint8_t pay2[kFeatureSlotPayloadBytes]; make_payload(pay2, kFeatureSlotPayloadBytes, 2);

    writer.publish(iid, 100u, 1000u, 0x1u, 1u, pay1, kFeatureSlotPayloadBytes);
    writer.publish(iid, 200u, 2000u, 0x3u, 2u, pay2, kFeatureSlotPayloadBytes);

    FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(iid, view));

    // Must see the second publication.
    EXPECT_EQ(view.source_seq,    200u);
    EXPECT_EQ(view.pub_ts_ns,     2000u);
    EXPECT_EQ(view.validity_mask, 0x3u);
    EXPECT_EQ(view.payload_count, 2u);
    EXPECT_EQ(0, std::memcmp(view.payload, pay2, kFeatureSlotPayloadBytes));

    free_region(region);
}

// ── T03: unpublished slot rejected ───────────────────────────────────────────
TEST(FeatureTransportTest, T03_UnpublishedSlotRejected) {
    FeatureRegion* region = alloc_region();
    FeatureReader  reader(region);

    FeatureSlotView view{};
    // No publish has happened; sequence == 0.
    EXPECT_FALSE(reader.try_read(5u, view))
        << "try_read must return false for never-published slot";

    free_region(region);
}

// ── T04: odd-sequence (torn write) rejected ───────────────────────────────────
TEST(FeatureTransportTest, T04_OddSequenceRejected) {
    FeatureRegion* region = alloc_region();

    // Manually set sequence to odd (simulating a writer crash mid-write).
    region->slots[3].header.sequence.store(1u, std::memory_order_relaxed);

    FeatureReader   reader(region);
    FeatureSlotView view{};
    EXPECT_FALSE(reader.try_read(3u, view))
        << "try_read must reject odd sequence (write in progress)";

    free_region(region);
}

// ── T05: schema version mismatch rejected ─────────────────────────────────────
TEST(FeatureTransportTest, T05_SchemaVersionMismatchRejected) {
    FeatureRegion* region = alloc_region();

    // Publish with a well-formed seqlock but wrong schema_version.
    FeatureSlot& slot = region->slots[10];
    slot.header.sequence.fetch_add(1, std::memory_order_seq_cst);
    slot.header.instrument_id  = 10u;
    slot.header.schema_version = 0xFFu;  // future/unknown version
    slot.header.source_seq     = 1u;
    slot.header.pub_ts_ns      = 1u;
    slot.header.validity_mask  = 0u;
    slot.header.payload_count  = 0u;
    slot.header.payload_bytes  = 0u;
    std::memset(slot.header.reserved_b, 0, sizeof(slot.header.reserved_b));
    std::memset(slot.payload, 0, kFeatureSlotPayloadBytes);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    slot.header.sequence.fetch_add(1, std::memory_order_seq_cst);

    FeatureReader   reader(region);
    FeatureSlotView view{};
    EXPECT_FALSE(reader.try_read(10u, view))
        << "try_read must reject unknown schema_version";

    free_region(region);
}

// ── T06: source_seq preserved ────────────────────────────────────────────────
TEST(FeatureTransportTest, T06_SourceSeqPreserved) {
    FeatureRegion* region = alloc_region();
    FeaturePublisher writer(region);
    FeatureReader    reader(region);

    const uint64_t expected_src = 0xDEAD'BEEF'CAFE'BABEull;
    writer.publish(1u, expected_src, 0u, 0u, 0u, nullptr, 0u);

    FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(1u, view));
    EXPECT_EQ(view.source_seq, expected_src);

    free_region(region);
}

// ── T07: two instruments are independent ─────────────────────────────────────
TEST(FeatureTransportTest, T07_TwoInstrumentsAreIndependent) {
    FeatureRegion* region = alloc_region();
    FeaturePublisher writer(region);
    FeatureReader    reader(region);

    uint8_t pay_a[kFeatureSlotPayloadBytes]; make_payload(pay_a, kFeatureSlotPayloadBytes, 0xAA);
    uint8_t pay_b[kFeatureSlotPayloadBytes]; make_payload(pay_b, kFeatureSlotPayloadBytes, 0xBB);

    writer.publish( 0u, 1u, 100u, 0x1u, 1u, pay_a, kFeatureSlotPayloadBytes);
    writer.publish(99u, 2u, 200u, 0x2u, 2u, pay_b, kFeatureSlotPayloadBytes);

    FeatureSlotView va{}, vb{};
    ASSERT_TRUE(reader.try_read( 0u, va));
    ASSERT_TRUE(reader.try_read(99u, vb));

    // Instrument 0 must not be contaminated by instrument 99.
    EXPECT_EQ(va.instrument_id, 0u);
    EXPECT_EQ(va.source_seq,   1u);
    EXPECT_EQ(va.validity_mask, 0x1u);
    EXPECT_EQ(0, std::memcmp(va.payload, pay_a, kFeatureSlotPayloadBytes));

    EXPECT_EQ(vb.instrument_id, 99u);
    EXPECT_EQ(vb.source_seq,   2u);
    EXPECT_EQ(vb.validity_mask, 0x2u);
    EXPECT_EQ(0, std::memcmp(vb.payload, pay_b, kFeatureSlotPayloadBytes));

    free_region(region);
}

// ── T08: concurrent one-writer / one-reader (10 000 iterations) ───────────────
// Verifies that the seqlock protocol prevents the reader from ever observing
// a torn state.  The reader may see an old or a new value but never a mix.
TEST(FeatureTransportTest, T08_ConcurrentWriterReader) {
    FeatureRegion* region = alloc_region();
    FeaturePublisher writer(region);
    FeatureReader    reader(region);

    constexpr int    N_ITERS   = 10'000;
    constexpr uint32_t IID     = 42u;
    constexpr uint32_t PB      = 8u; // use only first 8 payload bytes for speed

    std::atomic<bool> done{false};
    std::atomic<int>  torn_reads{0};

    // Writer thread: publishes N_ITERS times with source_seq and a matching
    // canary in the payload (first 8 bytes mirror the source_seq).
    std::thread writer_thread([&]{
        uint8_t pay[kFeatureSlotPayloadBytes]{};
        for (int i = 0; i < N_ITERS; ++i) {
            const uint64_t seq = static_cast<uint64_t>(i + 1);
            std::memcpy(pay, &seq, sizeof(seq));
            writer.publish(IID, seq, 0u, 0u, 0u, pay, PB);
        }
        done.store(true, std::memory_order_release);
    });

    // Reader thread: spins until done; on each successful read verifies that
    // the canary in payload[0..7] matches source_seq (consistency invariant).
    std::thread reader_thread([&]{
        FeatureSlotView view{};
        while (!done.load(std::memory_order_acquire)) {
            if (reader.try_read(IID, view)) {
                // Canary: first 8 payload bytes must equal source_seq.
                uint64_t canary = 0;
                std::memcpy(&canary, view.payload, sizeof(canary));
                // Only check if canary was written (source_seq > 0 means writer
                // wrote the canary).
                if (view.source_seq > 0 && view.payload_bytes >= sizeof(canary)) {
                    if (canary != view.source_seq) {
                        torn_reads.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }
    });

    writer_thread.join();
    reader_thread.join();

    EXPECT_EQ(torn_reads.load(), 0)
        << "seqlock must prevent torn reads (canary mismatch)";

    // Final state must be readable and show the last write.
    FeatureSlotView final_view{};
    ASSERT_TRUE(reader.try_read(IID, final_view));
    EXPECT_EQ(final_view.source_seq, static_cast<uint64_t>(N_ITERS));

    free_region(region);
}

// ── T09: out-of-range instrument_id rejected ─────────────────────────────────
TEST(FeatureTransportTest, T09_OutOfRangeInstrumentIdRejected) {
    FeatureRegion* region = alloc_region();
    FeaturePublisher writer(region);
    FeatureReader    reader(region);

    const uint32_t oob = static_cast<uint32_t>(kMaxInstruments); // == 256
    EXPECT_FALSE(writer.publish(oob, 0u, 0u, 0u, 0u, nullptr, 0u))
        << "publish must reject out-of-range instrument_id";

    FeatureSlotView view{};
    EXPECT_FALSE(reader.try_read(oob, view))
        << "try_read must reject out-of-range instrument_id";

    free_region(region);
}

// ── T10: zero-byte payload ────────────────────────────────────────────────────
TEST(FeatureTransportTest, T10_ZeroBytePayloadIsZeroed) {
    FeatureRegion* region = alloc_region();
    FeaturePublisher writer(region);
    FeatureReader    reader(region);

    // Publish with no payload (skeleton-only).
    ASSERT_TRUE(writer.publish(0u, 0u, 0u, 0u, 0u, nullptr, 0u));

    FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(0u, view));

    EXPECT_EQ(view.payload_bytes, 0u);
    EXPECT_EQ(view.validity_mask, 0u);
    EXPECT_EQ(view.payload_count, 0u);

    // Entire payload must be zeroed.
    uint8_t expected_zero[kFeatureSlotPayloadBytes]{};
    EXPECT_EQ(0, std::memcmp(view.payload, expected_zero, kFeatureSlotPayloadBytes))
        << "zero-byte publish must leave payload zeroed";

    free_region(region);
}

// ── T11: oversize payload rejected ────────────────────────────────────────────
TEST(FeatureTransportTest, T11_OversizedPayloadRejected) {
    FeatureRegion* region = alloc_region();
    FeaturePublisher writer(region);
    FeatureReader    reader(region);

    uint8_t large_payload[kFeatureSlotPayloadBytes + 1]{};

    // Attempting to publish an oversized payload must return false.
    EXPECT_FALSE(writer.publish(0u, 0u, 0u, 0u, 0u, large_payload, kFeatureSlotPayloadBytes + 1))
        << "publish must reject oversized payloads";

    FeatureSlotView view{};
    // The slot must remain unpublished.
    EXPECT_FALSE(reader.try_read(0u, view))
        << "try_read must reject slot because it was not published";

    free_region(region);
}

// ── T12: malformed payload_bytes rejected by reader ───────────────────────────
TEST(FeatureTransportTest, T12_MalformedPayloadBytesRejected) {
    FeatureRegion* region = alloc_region();

    // Simulate a corrupted/malformed slot directly in SHM where payload_bytes
    // is artificially larger than kFeatureSlotPayloadBytes.
    FeatureSlot& slot = region->slots[0];
    slot.header.sequence.fetch_add(1, std::memory_order_seq_cst);
    slot.header.instrument_id  = 0u;
    slot.header.schema_version = kFeatureSlotSchemaVersion;
    slot.header.source_seq     = 1u;
    slot.header.pub_ts_ns      = 1u;
    slot.header.validity_mask  = 0u;
    slot.header.payload_count  = 0u;
    slot.header.payload_bytes  = kFeatureSlotPayloadBytes + 1; // Malformed!
    std::memset(slot.header.reserved_b, 0, sizeof(slot.header.reserved_b));
    std::memset(slot.payload, 0, kFeatureSlotPayloadBytes);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    slot.header.sequence.fetch_add(1, std::memory_order_seq_cst);

    FeatureReader reader(region);
    FeatureSlotView view{};

    // The reader must detect the oversized payload_bytes and reject the read
    EXPECT_FALSE(reader.try_read(0u, view))
        << "try_read must reject slot with malformed payload_bytes";

    free_region(region);
}
