#include "engine/l2/feature_engine.h"
#include "engine/l1/lob_snapshot_publisher.h"
#include <gtest/gtest.h>
#include <memory>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

namespace {

TEST(FeatureEngineTest, ProcessValidInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    auto feature_region = std::make_unique<egbos::shm::FeatureVectorRegion>();

    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());

    egbos::l1::Top5BidsAsks expected{};
    expected.bid_qtys[0] = 111;
    ASSERT_TRUE(publisher.publish(7, expected));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region.get());

    ASSERT_TRUE(engine.process_instrument(7));

    // Verify L3 FeatureVectorRegion was written
    uint8_t out[egbos::shm::kFeatureVectorBytes];
    feature_region->read(out);

    // For now we just check it is zero initialized
    uint8_t zeroes[egbos::shm::kFeatureVectorBytes]{};
    EXPECT_EQ(std::memcmp(out, zeroes, sizeof(zeroes)), 0);
}

TEST(FeatureEngineTest, FailsOnUnpublishedInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    auto feature_region = std::make_unique<egbos::shm::FeatureVectorRegion>();
    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region.get());

    // Never published, sequence is 0, reader must reject it.
    EXPECT_FALSE(engine.process_instrument(3));
}

TEST(FeatureEngineTest, SucceedsOnZeroValuedPublishedInstrument) {
    auto lob_region = std::make_unique<egbos::shm::LobSnapshotsRegion>();
    auto feature_region = std::make_unique<egbos::shm::FeatureVectorRegion>();
    egbos::l1::LobSnapshotPublisher publisher(lob_region.get());

    egbos::l1::Top5BidsAsks expected{}; // all zeros
    ASSERT_TRUE(publisher.publish(7, expected));

    egbos::l2::FeatureEngine engine(lob_region.get(), feature_region.get());
    // Legitimate publish of all-zeros should be valid (seq > 0)
    EXPECT_TRUE(engine.process_instrument(7));
}

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
