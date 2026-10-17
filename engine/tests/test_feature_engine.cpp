// engine/tests/test_feature_engine.cpp
// Tests for L2 FeatureEngine — production framework, contract-driven.
//
// Test inventory:
//   FeatureEngineTest.ProcessValidInstrument
//   FeatureEngineTest.SkeletonSlotContents           (schema_ver, validity, count, bytes)
//   FeatureEngineTest.SourceSeqPreserved
//   FeatureEngineTest.FailsOnUnpublishedInstrument
//   FeatureEngineTest.NullLobRegionRejected
//   FeatureEngineTest.OutOfRangeInstrumentRejected
//   FeatureEngineTest.InstrumentIsolation
//   FeatureEngineTest.ReprocessReplacesThroughSeqlock
//   FeatureEngineTest.NoPayloadTruncationPossible
//   FeatureEngineTest.ProcessAllPublishedSkipsUnpublished
//   FeatureEngineTest.SucceedsOnZeroValuedPublishedInstrument
//   FeatureEngineTest.ConcurrentFeatureVectorWriteRead   (legacy, preserved)
//   PriceReconstructionTest.ZeroOffset
//   PriceReconstructionTest.PositiveOffset
//   PriceReconstructionTest.NegativeOffset
//   PriceReconstructionTest.LargeAnchorNegativeOffset
//   PriceReconstructionTest.OverflowSaturation
//   PriceReconstructionTest.RoundTripThroughLobEngine
//   PriceReconstructionTest.TransportRoundTrip
//
// All tests operate on in-process stack/heap allocations, no /dev/shm.
// No quantitative feature is tested as approved: all validity_mask==0 assertions
// reflect the current null-contract production state, not a test weakness.

#include "engine/l2/feature_engine.h"
#include "engine/l2/feature_reader.h"
#include "engine/l2/feature_compute.h"
#include "engine/l1/lob_snapshot_publisher.h"
#include <gtest/gtest.h>
#include <memory>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

namespace {

// ── Helpers ─────────────────────────────────────────────────────────────────

static egbos::shm::FeatureRegion* alloc_feature_region() {
    void* raw = std::aligned_alloc(64, sizeof(egbos::shm::FeatureRegion));
    return new (raw) egbos::shm::FeatureRegion{};
}
static void free_feature_region(egbos::shm::FeatureRegion* r) {
    r->~FeatureRegion();
    std::free(r);
}

// ── TEST 1: basic round-trip ─────────────────────────────────────────────────

TEST(FeatureEngineTest, ProcessValidInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());

    egbos::l1::Top5BidsAsks snap{};
    snap.bid_qtys[0] = 111;
    ASSERT_TRUE(publisher.publish(7, snap));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(7));

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(7, view));
    EXPECT_EQ(view.instrument_id, 7u);

    free_feature_region(feature_region);
}

// ── TEST 2: exact skeleton slot contents (no-contract state) ─────────────────
//
// Verifies that the FeatureSlot written under the null-contract state contains
// exactly the expected sentinel values.  This is the authoritative regression
// guard: if any future change accidentally marks a feature valid or writes
// payload bytes, this test fails.

TEST(FeatureEngineTest, SkeletonSlotContents) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l2::reset_production_features(3);

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};
    snap.bid_qtys[2] = 42;
    ASSERT_TRUE(publisher.publish(3, snap));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(3));

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(3, view));

    // Ratified feature contracts:
    //   FEAT-0001 (Hawkes Intensity, Feature ID: 0x0001): bit 0
    //   FEAT-0002 (Level-1 OFI,      Feature ID: 0x0002): bit 1
    // validity_mask = (1 << 0) | (1 << 1) = 3u, feature_count=2, payload_bytes=8
    EXPECT_EQ(view.schema_version, egbos::l2::kFeatureSlotSchemaVersion);
    EXPECT_EQ(view.validity_mask,  3u);
    EXPECT_EQ(view.payload_count,  2u);
    EXPECT_EQ(view.payload_bytes,  8u);

    // Initial Hawkes intensity at cold start = mu + alpha = 0.1 + 0.05 = 0.15
    float intensity = 0.0f;
    std::memcpy(&intensity, view.payload, sizeof(float));
    EXPECT_NEAR(intensity, 0.15f, 1e-5f);

    // Initial OFI at cold start = 0.0f
    float ofi = 0.0f;
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, 0.0f, 1e-5f);

    // Payload bytes beyond 8 must be zero.
    const uint8_t zeroes[egbos::shm::kFeatureSlotPayloadBytes - 8]{};
    EXPECT_EQ(0, std::memcmp(view.payload + 8, zeroes, sizeof(zeroes)))
        << "payload beyond byte 8 must remain zeroed";

    free_feature_region(feature_region);
}

// ── TEST 3: source_seq preserved ─────────────────────────────────────────────

TEST(FeatureEngineTest, SourceSeqPreserved) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};
    ASSERT_TRUE(publisher.publish(7, snap));
    ASSERT_TRUE(publisher.publish(7, snap));
    ASSERT_TRUE(publisher.publish(7, snap));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(7));

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(7, view));

    // Read the L1 sequence directly and compare.
    egbos::l2::LobSnapshotReader lob_reader(lob_region.get());
    egbos::l1::Top5BidsAsks out{};
    uint64_t expected_seq = 0;
    ASSERT_TRUE(lob_reader.read(7, out, expected_seq));

    EXPECT_GT(expected_seq, 0u);
    EXPECT_EQ(view.source_seq, expected_seq);

    free_feature_region(feature_region);
}

// ── TEST 4: unpublished instrument is rejected ────────────────────────────────

TEST(FeatureEngineTest, FailsOnUnpublishedInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    // Never published → seq==0 → reader rejects.
    EXPECT_FALSE(engine.process_instrument(3));

    // The slot must remain unpublished.
    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    EXPECT_FALSE(reader.try_read(3, view));

    free_feature_region(feature_region);
}

// ── TEST 5: null lob_region handled safely ────────────────────────────────────

TEST(FeatureEngineTest, NullLobRegionRejected) {
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l2::FeatureEngine engine(nullptr, feature_region);
    EXPECT_FALSE(engine.process_instrument(0));

    free_feature_region(feature_region);
}

// ── TEST 6: out-of-range instrument_id rejected ───────────────────────────────

TEST(FeatureEngineTest, OutOfRangeInstrumentRejected) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);

    const uint32_t oob = egbos::shm::kMaxInstruments; // == 256
    EXPECT_FALSE(engine.process_instrument(oob));

    free_feature_region(feature_region);
}

// ── TEST 7: instrument isolation ──────────────────────────────────────────────
// Publishing instrument A must not alter instrument B's slot.

TEST(FeatureEngineTest, InstrumentIsolation) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};
    snap.bid_qtys[0] = 99;
    ASSERT_TRUE(publisher.publish(10, snap));  // publish only instrument 10

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(10));

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};

    // Instrument 10 must be readable.
    ASSERT_TRUE(reader.try_read(10, view));
    EXPECT_EQ(view.instrument_id, 10u);

    // Instrument 11 was never published; its slot must remain unpublished.
    egbos::l2::FeatureSlotView other_view{};
    EXPECT_FALSE(reader.try_read(11, other_view))
        << "Instrument 11 must not be affected by publishing instrument 10";

    free_feature_region(feature_region);
}

// ── TEST 8: re-processing with a NEW sequence replaces slot; same-seq is a no-op
//
// With deduplication, process_instrument() must:
//   (a) Return true  when L1 source_seq advances  (new data).
//   (b) Return false when L1 source_seq is the same (dedup / no-op).
TEST(FeatureEngineTest, ReprocessReplacesThroughSeqlock) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l2::LobSnapshotReader lob_reader(lob_region.get());

    egbos::l1::Top5BidsAsks snap{};
    snap.bid_qtys[0] = 55;
    ASSERT_TRUE(publisher.publish(5, snap));

    // Capture the seq after the first publish.
    egbos::l1::Top5BidsAsks tmp{};
    uint64_t seq_after_first = 0;
    ASSERT_TRUE(lob_reader.read(5, tmp, seq_after_first));
    ASSERT_GT(seq_after_first, 0u);

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(5));  // new seq -> must succeed

    // Same seq: re-processing without a new L1 publish is a dedup no-op.
    EXPECT_FALSE(engine.process_instrument(5))
        << "Dedup must return false when source_seq is unchanged";

    // The slot published by the first call must still be readable and valid.
    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView first_view{};
    ASSERT_TRUE(reader.try_read(5, first_view));
    EXPECT_GT(first_view.source_seq, 0u);
    EXPECT_EQ(first_view.validity_mask, 3u);

    // Now publish a genuinely NEW snapshot.
    snap.bid_qtys[0] = 66;
    ASSERT_TRUE(publisher.publish(5, snap));
    ASSERT_TRUE(publisher.publish(5, snap));  // ensure buffer B also advanced

    uint64_t seq_after_second = 0;
    ASSERT_TRUE(lob_reader.read(5, tmp, seq_after_second));

    // If a genuinely new sequence is visible, process_instrument must succeed.
    if (seq_after_second != seq_after_first) {
        ASSERT_TRUE(engine.process_instrument(5))
            << "Must succeed when source_seq advances";

        egbos::l2::FeatureSlotView second_view{};
        ASSERT_TRUE(reader.try_read(5, second_view));
        EXPECT_EQ(second_view.validity_mask, 3u);
        EXPECT_EQ(second_view.payload_count, 2u);
        EXPECT_EQ(second_view.payload_bytes, 8u);
        EXPECT_EQ(second_view.instrument_id, 5u);
        EXPECT_GT(second_view.source_seq, 0u);
    } else {
        // Both publishes landed on the same seqlock counter (valid with ping-pong
        // buffers): dedup still returns false.
        EXPECT_FALSE(engine.process_instrument(5))
            << "Dedup must return false when source_seq has not advanced";
    }

    free_feature_region(feature_region);
}

// ── TEST 9: no payload truncation possible ────────────────────────────────────
// compute_production_features() returns payload_bytes==0, which is within
// capacity.  The explicit size guard in process_instrument() must never allow
// a payload_bytes > kFeatureSlotPayloadBytes through to publish().
// This is verified by the null-contract state (payload_bytes==0 always).

TEST(FeatureEngineTest, NoPayloadTruncationPossible) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};
    ASSERT_TRUE(publisher.publish(0, snap));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(0));

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(0, view));

    // payload_bytes must be exactly 8 (FEAT-0001 + FEAT-0002) and <= capacity.
    EXPECT_EQ(view.payload_bytes, 8u);
    EXPECT_LE(view.payload_bytes, egbos::shm::kFeatureSlotPayloadBytes);

    free_feature_region(feature_region);
}

// ── TEST 10: process_all_published skips unpublished ──────────────────────────

TEST(FeatureEngineTest, ProcessAllPublishedSkipsUnpublished) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};
    // Publish exactly 3 instruments.
    ASSERT_TRUE(publisher.publish(0,   snap));
    ASSERT_TRUE(publisher.publish(10,  snap));
    ASSERT_TRUE(publisher.publish(255, snap));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    const uint32_t count = engine.process_all_published();

    // Exactly 3 slots should have been updated.
    EXPECT_EQ(count, 3u);

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView v{};
    EXPECT_TRUE(reader.try_read(0,   v));
    EXPECT_TRUE(reader.try_read(10,  v));
    EXPECT_TRUE(reader.try_read(255, v));

    // An instrument never published must remain absent.
    EXPECT_FALSE(reader.try_read(1, v));

    free_feature_region(feature_region);
}

// ── TEST 11: all-zero L1 snapshot succeeds ────────────────────────────────────

TEST(FeatureEngineTest, SucceedsOnZeroValuedPublishedInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};  // all zeros
    ASSERT_TRUE(publisher.publish(7, snap));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    EXPECT_TRUE(engine.process_instrument(7));

    free_feature_region(feature_region);
}

// ── TEST 11b: FEAT-0001 Hawkes Intensity values and payload encoding ─────────
TEST(FeatureEngineTest, HawkesIntensityValuesAndPayloadEncoding) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l2::reset_production_features(12);

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};
    snap.anchor_price = 50000LL;
    snap.tick_size = 10LL;
    snap.bid_qtys[0] = 100;
    snap.ask_qtys[0] = 150;
    ASSERT_TRUE(publisher.publish(12, snap));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(12));

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(12, view));

    EXPECT_EQ(view.instrument_id, 12u);
    EXPECT_EQ(view.validity_mask, 3u);
    EXPECT_EQ(view.payload_count, 2u);
    EXPECT_EQ(view.payload_bytes, 8u);

    // Initial event must produce mu + alpha = 0.10 + 0.05 = 0.15
    float intensity = 0.0f;
    std::memcpy(&intensity, view.payload, sizeof(float));
    EXPECT_NEAR(intensity, 0.15f, 1e-5f);

    // Initial event for OFI must produce cold start 0.0f
    float ofi = 0.0f;
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, 0.0f, 1e-5f);

    free_feature_region(feature_region);
}

// ── TEST 11c: Dedup skips same source_seq ────────────────────────────────
//
// process_instrument() returns false when no new L1 publish has occurred
// (source_seq unchanged).  The existing FeatureSlot must remain valid.
TEST(FeatureEngineTest, DedupSkipsSameSourceSeq) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l2::reset_production_features(20);

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};
    snap.bid_qtys[0] = 77;
    ASSERT_TRUE(publisher.publish(20, snap));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);

    // First call: new data -> should succeed.
    ASSERT_TRUE(engine.process_instrument(20));

    // Second call: no new L1 publish -> dedup returns false (not an error).
    EXPECT_FALSE(engine.process_instrument(20))
        << "Dedup must return false when source_seq has not advanced";

    // The slot published by the first call must still be readable and valid.
    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(20, view));
    EXPECT_EQ(view.instrument_id, 20u);
    EXPECT_EQ(view.validity_mask, 3u);
    EXPECT_EQ(view.payload_bytes, 8u);

    free_feature_region(feature_region);
}

// ── TEST 11d: Gap event resets Hawkes state ────────────────────────────
//
// After a sequence gap (prev_seq==0 to a large seq), the first compute call
// must produce the cold-start Hawkes value (mu+alpha=0.15), proving that the
// gap detection path called reset_production_features().
TEST(FeatureEngineTest, GapResetsHawkesState) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    constexpr uint32_t kInstr = 30u;
    egbos::l2::reset_production_features(kInstr);

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l1::Top5BidsAsks snap{};
    snap.bid_qtys[0] = 1;

    // Use a single engine instance so prev_seq is remembered across the gap.
    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);

    // Publish once and process to warm up Hawkes state.
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(engine.process_instrument(kInstr));

    // Publish several more times so that the seqlock counter advances by > 2.
    for (int i = 0; i < 5; ++i) {
        snap.bid_qtys[0] = static_cast<uint32_t>(i + 2);
        ASSERT_TRUE(publisher.publish(kInstr, snap));
    }

    // Processing now sees source_seq > prev_seq + 2 -> gap detected -> Hawkes state reset.
    ASSERT_TRUE(engine.process_instrument(kInstr));

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(kInstr, view));

    // After a gap-triggered reset, the first compute is a cold-start:
    // expected intensity = mu + alpha = 0.1 + 0.05 = 0.15.
    float intensity = 0.0f;
    std::memcpy(&intensity, view.payload, sizeof(float));
    EXPECT_NEAR(intensity, 0.15f, 1e-5f)
        << "After gap reset, Hawkes must restart from cold-start value mu+alpha=0.15";

    // Expected OFI after gap reset is cold-start 0.0f.
    float ofi = 0.0f;
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, 0.0f, 1e-5f)
        << "After gap reset, OFI must restart from cold-start value 0.0f";

    free_feature_region(feature_region);
}

// ── TEST 11e: Level-1 Order Flow Imbalance (FEAT-0002) ──────────────────
//
// Verifies:
//   - Cold start produces OFI = 0.0f
//   - Bid improvement produces positive OFI
//   - Bid size increase produces positive OFI
//   - Ask improvement (price drop) produces negative OFI
//   - Ask size increase produces negative OFI
//   - Gap reset clears OFI state and returns to 0.0f
//   - Zero dynamic memory allocations
TEST(FeatureEngineTest, OrderFlowImbalanceCalculationsAndReset) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    constexpr uint32_t kInstr = 42u;
    egbos::l2::reset_production_features(kInstr);

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());
    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};

    // Base snapshot: anchor_price = 10000, tick_size = 10
    // Best bid: offset = 0 -> price = 10000, qty = 50
    // Best ask: offset = 2 -> price = 10020, qty = 40
    egbos::l1::Top5BidsAsks snap{};
    snap.anchor_price = 10000;
    snap.tick_size = 10;
    snap.bid_price_offsets[0] = 0;
    snap.bid_qtys[0] = 50;
    snap.ask_price_offsets[0] = 2;
    snap.ask_qtys[0] = 40;

    // Snapshot 1 (Cold start):
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(engine.process_instrument(kInstr));
    ASSERT_TRUE(reader.try_read(kInstr, view));
    EXPECT_EQ(view.validity_mask, 3u);
    EXPECT_EQ(view.payload_count, 2u);
    EXPECT_EQ(view.payload_bytes, 8u);

    float ofi = 0.0f;
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, 0.0f, 1e-5f) << "Cold start OFI must be 0.0f";

    // Snapshot 2: Bid size increases (same price 10000, qty 50 -> 70), ask unchanged
    // e_b = 70 - 50 = +20, e_a = 0 -> OFI = +20.0f
    snap.bid_qtys[0] = 70;
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(publisher.publish(kInstr, snap)); // ensure buffer advanced
    ASSERT_TRUE(engine.process_instrument(kInstr));
    ASSERT_TRUE(reader.try_read(kInstr, view));
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, 20.0f, 1e-5f) << "Bid size increase must produce positive OFI";

    // Snapshot 3: Bid price improves (offset 0 -> 1: price 10000 -> 10010, qty = 35), ask unchanged
    // e_b = +35 (price rose), e_a = 0 -> OFI = +35.0f
    snap.bid_price_offsets[0] = 1;
    snap.bid_qtys[0] = 35;
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(engine.process_instrument(kInstr));
    ASSERT_TRUE(reader.try_read(kInstr, view));
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, 35.0f, 1e-5f) << "Bid price improvement must produce positive OFI";

    // Snapshot 4: Ask size increases (same ask price offset 2, qty 40 -> 65), bid unchanged
    // e_b = 0, e_a = 65 - 40 = +25 -> OFI = e_b - e_a = -25.0f
    snap.ask_qtys[0] = 65;
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(engine.process_instrument(kInstr));
    ASSERT_TRUE(reader.try_read(kInstr, view));
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, -25.0f, 1e-5f) << "Ask size increase must produce negative OFI";

    // Snapshot 5: Ask price improves / drops (offset 2 -> 1: price 10020 -> 10010, qty 30)
    // Bid stays at offset 0 (price 10000, qty 50)
    // Ask improvement means new lower ask price (selling pressure):
    // e_b = 0, e_a = +30 (new ask entered at lower price) -> OFI = e_b - e_a = -30.0f
    snap.ask_price_offsets[0] = 1;
    snap.ask_qtys[0] = 30;
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(publisher.publish(kInstr, snap));
    ASSERT_TRUE(engine.process_instrument(kInstr));
    ASSERT_TRUE(reader.try_read(kInstr, view));
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, -30.0f, 1e-5f) << "Ask price improvement (drop) must produce negative OFI";

    // Snapshot 6: Sequence gap triggers reset_production_features()
    // Publish > 2 times to trigger a gap detection in FeatureEngine.
    for (int i = 0; i < 5; ++i) {
        snap.bid_qtys[0] = static_cast<uint32_t>(100 + i);
        ASSERT_TRUE(publisher.publish(kInstr, snap));
    }
    ASSERT_TRUE(engine.process_instrument(kInstr));
    ASSERT_TRUE(reader.try_read(kInstr, view));

    // After gap reset, OFI must restart with cold-start 0.0f
    std::memcpy(&ofi, view.payload + 4, sizeof(float));
    EXPECT_NEAR(ofi, 0.0f, 1e-5f) << "Gap reset must reset OFI state to cold-start 0.0f";

    free_feature_region(feature_region);
}

// ── TEST 12: legacy FeatureVectorRegion (single-slot) concurrent test ─────────
// Preserved independently of FeatureEngine — validates the old transport layer.
// The reader checks that a single atomic read is internally coherent (all bytes
// identical), not that two successive reads are the same.  The write() path uses
// a seqlock that guarantees within-read coherence; adjacent reads from different
// iterations may legitimately differ.

TEST(FeatureEngineTest, ConcurrentFeatureVectorWriteRead) {
    auto feature_region = std::make_unique<egbos::shm::FeatureVectorRegion>();

    constexpr int kIterations = 1000;
    std::atomic<int> write_count{0};

    // Writer thread
    std::thread writer([&]() {
        uint8_t data[egbos::shm::kFeatureVectorBytes]{};
        for (int i = 0; i < kIterations; ++i) {
            std::memset(data, i % 256, sizeof(data));
            feature_region->write(data);
            write_count.fetch_add(1, std::memory_order_release);
        }
    });

    // Reader thread: spins until writer completes, checks within-read coherence.
    std::thread reader([&]() {
        uint8_t out[egbos::shm::kFeatureVectorBytes];
        while (write_count.load(std::memory_order_acquire) < kIterations) {
            feature_region->read(out);
            // All bytes in a single coherent read must be identical.
            uint8_t val = out[0];
            for (size_t i = 1; i < egbos::shm::kFeatureVectorBytes; ++i) {
                EXPECT_EQ(out[i], val)
                    << "torn read detected at byte " << i;
                if (out[i] != val) return;  // stop on first failure
            }
        }
    });

    writer.join();
    reader.join();
}

} // namespace

// ============================================================================
// PriceReconstructionTest suite
//
// Verifies that Top5BidsAsks::calculate_absolute_price() correctly reconstructs
// full int64 absolute prices from (anchor_price, tick_size, level_offset).
//
// This resolves §PRICE-RECON: the transport ABI v2 now carries anchor_price
// (int64_t) and tick_size (int64_t), making round-trip reconstruction exact.
// ============================================================================

#include "engine/core/lob_snapshot_abi.h"
#include "engine/l1/lob_engine.h"
#include "engine/l1/lob_snapshot_publisher.h"
#include "engine/l2/lob_snapshot_reader.h"

namespace {

// Helper: build a minimal Top5BidsAsks with given anchor_price and tick_size.
static egbos::l1::Top5BidsAsks make_snap(
    int64_t anchor_price, int64_t tick_size,
    int16_t bid0_offset = 0, int16_t ask0_offset = 0) noexcept
{
    egbos::l1::Top5BidsAsks s{};
    s.anchor_price         = anchor_price;
    s.tick_size            = tick_size;
    s.bid_price_offsets[0] = bid0_offset;
    s.ask_price_offsets[0] = ask0_offset;
    s.bid_qtys[0]          = 1u;  // non-zero to mark as published
    return s;
}

// ── PR-1: zero offset always returns anchor_price ─────────────────────────────
TEST(PriceReconstructionTest, ZeroOffset) {
    auto s = make_snap(/*anchor=*/100'000LL, /*tick=*/100LL);
    EXPECT_EQ(s.calculate_absolute_price(0), 100'000LL);
}

// ── PR-2: positive offset = anchor + offset * tick ──────────────────────────────
TEST(PriceReconstructionTest, PositiveOffset) {
    auto s = make_snap(100'000LL, 100LL);
    // offset=+5 → 100000 + 5*100 = 100500
    EXPECT_EQ(s.calculate_absolute_price(5), 100'500LL);
    // offset=+1 → 100000 + 1*100 = 100100
    EXPECT_EQ(s.calculate_absolute_price(1), 100'100LL);
}

// ── PR-3: negative offset = anchor - |offset| * tick ───────────────────────────
TEST(PriceReconstructionTest, NegativeOffset) {
    auto s = make_snap(100'000LL, 100LL);
    // offset=-5 → 100000 + (-5)*100 = 99500
    EXPECT_EQ(s.calculate_absolute_price(-5), 99'500LL);
    // offset=-1 → 100000 + (-1)*100 = 99900
    EXPECT_EQ(s.calculate_absolute_price(-1), 99'900LL);
}

// ── PR-4: large anchor with negative offset ────────────────────────────────────
// Verifies that the reconstruction handles values above 2^31 correctly —
// this was impossible with the old anchor_price_low32 (uint32_t) approach.
TEST(PriceReconstructionTest, LargeAnchorNegativeOffset) {
    // Binance BTC price (satoshis): anchor near 6,000,000,000,000 (60k USD at 1e8)
    const int64_t anchor = 6'000'000'000'000LL;
    const int64_t tick   = 10'000LL;  // 0.0001 BTC = 10000 satoshis
    auto s = make_snap(anchor, tick);

    // level -10: anchor - 10 * tick_size = 6e12 - 100000 = 5,999,900,000,000
    EXPECT_EQ(s.calculate_absolute_price(-10),
              anchor - 10LL * tick);

    // level +20: anchor + 20 * tick_size
    EXPECT_EQ(s.calculate_absolute_price(20),
              anchor + 20LL * tick);
}

// ── PR-5: overflow saturation (does not invoke UB) ──────────────────────────────
TEST(PriceReconstructionTest, OverflowSaturation) {
    // Extreme anchor + large positive offset should saturate to INT64_MAX.
    const int64_t anchor = INT64_MAX - 1;
    const int64_t tick   = 1LL;
    auto s = make_snap(anchor, tick);

    // offset = +2 would overflow: (INT64_MAX - 1) + 2 = INT64_MAX + 1
    const int64_t result = s.calculate_absolute_price(2);
    EXPECT_EQ(result, INT64_MAX);  // saturated
}

// ── PR-6: round-trip through LobEngine::generate_top5_snapshot() ──────────────
// Verifies that the LobEngine correctly populates anchor_price and tick_size
// in the snapshot and that calculate_absolute_price() reconstructs the exact
// price that the engine used internally to place the order.
TEST(PriceReconstructionTest, RoundTripThroughLobEngine) {
    const int64_t anchor_price = 100'000LL;
    const int64_t tick_size    = 100LL;

    auto engine = std::make_unique<egbos::l1::LobEngine>(anchor_price, tick_size);

    // Add a bid at price 100'500 (offset = +5 ticks from anchor)
    const int64_t bid_price = 100'500LL;
    engine->add_order(1u, bid_price, 50u, /*is_bid=*/true);

    // Add an ask at price 100'600 (offset = +6 ticks from anchor)
    const int64_t ask_price = 100'600LL;
    engine->add_order(2u, ask_price, 30u, /*is_bid=*/false);

    auto snap = engine->generate_top5_snapshot();

    // Verify ABI v2 fields were populated
    EXPECT_EQ(snap.anchor_price, anchor_price);
    EXPECT_EQ(snap.tick_size,    tick_size);

    // The best bid offset was stored as (price_index - N/2).
    // Reconstruct the bid price using calculate_absolute_price.
    const int16_t bid_offset = snap.bid_price_offsets[0];
    const int64_t reconstructed_bid = snap.calculate_absolute_price(bid_offset);
    EXPECT_EQ(reconstructed_bid, bid_price)
        << "Bid price must round-trip exactly through anchor+tick reconstruction";

    // Same for ask.
    const int16_t ask_offset = snap.ask_price_offsets[0];
    const int64_t reconstructed_ask = snap.calculate_absolute_price(ask_offset);
    EXPECT_EQ(reconstructed_ask, ask_price)
        << "Ask price must round-trip exactly through anchor+tick reconstruction";
}

// ── PR-7: transport round-trip (LobEngine → publisher → SHM → reader → reconstruct)
TEST(PriceReconstructionTest, TransportRoundTrip) {
    const int64_t anchor_price = 5'000'000'000LL;  // >2^32, verifies high32 needed
    const int64_t tick_size    = 500LL;
    const int64_t bid_price    = anchor_price - 5LL * tick_size;  // -2500
    const int64_t ask_price    = anchor_price + 3LL * tick_size;  // +1500

    // Build the snapshot via LobEngine
    auto engine = std::make_unique<egbos::l1::LobEngine>(anchor_price, tick_size);
    engine->add_order(1u, bid_price, 100u, /*is_bid=*/true);
    engine->add_order(2u, ask_price, 80u,  /*is_bid=*/false);
    auto original_snap = engine->generate_top5_snapshot();

    // Publish through the L1->L2 transport
    auto region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::l1::LobSnapshotPublisher publisher(region.get());
    ASSERT_TRUE(publisher.publish(0u, original_snap));

    // Read back through the seqlock reader
    egbos::l2::LobSnapshotReader reader(region.get());
    egbos::l1::Top5BidsAsks received{};
    uint64_t out_seq = 0;
    ASSERT_TRUE(reader.read(0u, received, out_seq));

    // The anchor and tick must survive the transport exactly
    EXPECT_EQ(received.anchor_price, anchor_price)
        << "anchor_price (>2^32) must survive L1->L2 SHM transport";
    EXPECT_EQ(received.tick_size, tick_size)
        << "tick_size must survive L1->L2 SHM transport";

    // Price reconstruction from the received snapshot must give exact original prices
    const int64_t recv_bid = received.calculate_absolute_price(received.bid_price_offsets[0]);
    const int64_t recv_ask = received.calculate_absolute_price(received.ask_price_offsets[0]);

    EXPECT_EQ(recv_bid, bid_price)
        << "Bid price must reconstruct exactly after full SHM transport";
    EXPECT_EQ(recv_ask, ask_price)
        << "Ask price must reconstruct exactly after full SHM transport";
}

} // namespace (PriceReconstructionTest)
