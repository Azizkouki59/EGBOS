// L2 snapshot transport test.
//
// Proves:
//   - current 64B Top5 ABI
//   - inactive-buffer publication
//   - active-buffer flip
//   - coherent L2 readback
//
// LOCAL_SYNTHETIC only. No network, no orders, no real SHM required.

#include "engine/l1/lob_snapshot_publisher.h"
#include "engine/l2/lob_snapshot_reader.h"

#include <gtest/gtest.h>

#include <cstring>
#include <memory>

namespace {

TEST(L2SnapshotTransportTest, PublishAndReadExactTop5Payload) {
    auto region =
        std::make_unique<egbos::shm::LobSnapshotsRegion>();

    egbos::l1::LobSnapshotPublisher publisher(region.get());
    egbos::l2::LobSnapshotReader reader(region.get());

    egbos::l1::Top5BidsAsks expected{};
    expected.bid_qtys[0] = 111;
    expected.bid_qtys[1] = 222;
    expected.ask_qtys[0] = 333;
    expected.ask_qtys[1] = 444;
    expected.bid_price_offsets[0] = 12;
    expected.ask_price_offsets[0] = 13;
    expected.anchor_price          = 0x12345678LL;
    expected.tick_size              = 100LL;

    ASSERT_TRUE(publisher.publish(7, expected));

    ASSERT_EQ(
        region->instruments[7].header.active_buffer.load(
            std::memory_order_acquire),
        1u);

    egbos::l1::Top5BidsAsks actual{};
    uint64_t out_seq = 0;
    ASSERT_TRUE(reader.read(7, actual, out_seq));

    EXPECT_EQ(
        std::memcmp(&expected, &actual, sizeof(expected)),
        0);
}

TEST(L2SnapshotTransportTest, SecondPublishAlternatesBuffer) {
    auto region =
        std::make_unique<egbos::shm::LobSnapshotsRegion>();

    egbos::l1::LobSnapshotPublisher publisher(region.get());
    egbos::l2::LobSnapshotReader reader(region.get());

    egbos::l1::Top5BidsAsks first{};
    first.bid_qtys[0] = 100;

    egbos::l1::Top5BidsAsks second{};
    second.bid_qtys[0] = 200;

    ASSERT_TRUE(publisher.publish(3, first));
    ASSERT_EQ(
        region->instruments[3].header.active_buffer.load(
            std::memory_order_acquire),
        1u);

    ASSERT_TRUE(publisher.publish(3, second));
    ASSERT_EQ(
        region->instruments[3].header.active_buffer.load(
            std::memory_order_acquire),
        0u);

    egbos::l1::Top5BidsAsks actual{};
    uint64_t out_seq = 0;
    ASSERT_TRUE(reader.read(3, actual, out_seq));

    EXPECT_EQ(
        std::memcmp(&second, &actual, sizeof(second)),
        0);
}

TEST(L2SnapshotTransportTest, InvalidInstrumentIsRejected) {
    auto region =
        std::make_unique<egbos::shm::LobSnapshotsRegion>();

    egbos::l1::LobSnapshotPublisher publisher(region.get());
    egbos::l2::LobSnapshotReader reader(region.get());

    egbos::l1::Top5BidsAsks snapshot{};

    EXPECT_FALSE(
        publisher.publish(egbos::shm::kMaxInstruments, snapshot));

    egbos::l1::Top5BidsAsks out{};
    uint64_t out_seq = 0;
    EXPECT_FALSE(
        reader.read(egbos::shm::kMaxInstruments, out, out_seq));
}

} // namespace
