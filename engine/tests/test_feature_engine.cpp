// engine/tests/test_feature_engine.cpp
// Tests for L2 FeatureEngine → per-instrument FeatureRegion write path.
//
// The ConcurrentFeatureVectorWriteRead test is preserved from the legacy
// single-slot FeatureVectorRegion and validates that transport layer directly.
// The FeatureEngineTest cases now target the per-instrument FeatureRegion.

#include "engine/l2/feature_engine.h"
#include "engine/l2/feature_reader.h"
#include "engine/l1/lob_snapshot_publisher.h"
#include <gtest/gtest.h>
#include <memory>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

namespace {

// Helper: allocate a FeatureRegion with correct alignment.
static egbos::shm::FeatureRegion* alloc_feature_region() {
    void* raw = std::aligned_alloc(64, sizeof(egbos::shm::FeatureRegion));
    return new (raw) egbos::shm::FeatureRegion{};
}
static void free_feature_region(egbos::shm::FeatureRegion* r) {
    r->~FeatureRegion();
    std::free(r);
}

TEST(FeatureEngineTest, ProcessValidInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());

    egbos::l1::Top5BidsAsks expected{};
    expected.bid_qtys[0] = 111;
    ASSERT_TRUE(publisher.publish(7, expected));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(7));

    // Verify the per-instrument slot was published.
    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(7, view));
    EXPECT_EQ(view.instrument_id, 7u);
    EXPECT_EQ(view.validity_mask, 0u);  // no approved features yet

    free_feature_region(feature_region);
}

TEST(FeatureEngineTest, SourceSeqPreserved) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());

    egbos::l1::Top5BidsAsks expected{};
    ASSERT_TRUE(publisher.publish(7, expected)); // Sequence 1
    ASSERT_TRUE(publisher.publish(7, expected)); // Sequence 2
    ASSERT_TRUE(publisher.publish(7, expected)); // Sequence 3

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    ASSERT_TRUE(engine.process_instrument(7));

    egbos::l2::FeatureReader reader(feature_region);
    egbos::l2::FeatureSlotView view{};
    ASSERT_TRUE(reader.try_read(7, view));

    // The LobSnapshotPublisher uses double-buffered sequence protocol.
    // Publish 1: seq 2
    // Publish 2: seq 4
    // Publish 3: seq 6 (stored in the sequence counter).
    // Let's verify we got exactly what was in the LobSnapshotReader.
    egbos::l2::LobSnapshotReader lob_reader(lob_region.get());
    egbos::l1::Top5BidsAsks out{};
    uint64_t expected_seq = 0;
    ASSERT_TRUE(lob_reader.read(7, out, expected_seq));

    EXPECT_GT(expected_seq, 0u);
    EXPECT_EQ(view.source_seq, expected_seq);

    free_feature_region(feature_region);
}

TEST(FeatureEngineTest, FailsOnUnpublishedInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);

    // Never published to lob_region, sequence is 0, reader must reject it.
    EXPECT_FALSE(engine.process_instrument(3));

    free_feature_region(feature_region);
}

TEST(FeatureEngineTest, SucceedsOnZeroValuedPublishedInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    egbos::shm::FeatureRegion* feature_region = alloc_feature_region();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());

    egbos::l1::Top5BidsAsks expected{}; // all zeros
    ASSERT_TRUE(publisher.publish(7, expected));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region);
    // Legitimate publish of all-zeros should succeed (lob seq > 0).
    EXPECT_TRUE(engine.process_instrument(7));

    free_feature_region(feature_region);
}

// Legacy FeatureVectorRegion (single-slot) seqlock concurrent test.
// Preserved independently of FeatureEngine — validates the old transport layer.
TEST(FeatureEngineTest, ConcurrentFeatureVectorWriteRead) {
    auto feature_region = std::make_unique<egbos::shm::FeatureVectorRegion>();

    std::atomic<bool> run{true};

    // Writer thread simulating L2 output
    std::thread writer([&]() {
        uint8_t data[egbos::shm::kFeatureVectorBytes]{};
        for (int i = 0; i < 1000; ++i) {
            std::memset(data, i % 256, sizeof(data));
            feature_region->write(data);
        }
        run.store(false);
    });

    // Reader thread simulating L3 consumption
    std::thread reader([&]() {
        uint8_t out[egbos::shm::kFeatureVectorBytes];
        while (run.load()) {
            feature_region->read(out);
            // Should never see torn write, so all bytes should be identical
            uint8_t val = out[0];
            for (size_t i = 1; i < egbos::shm::kFeatureVectorBytes; ++i) {
                EXPECT_EQ(out[i], val);
            }
        }
    });

    writer.join();
    reader.join();
}

} // namespace
