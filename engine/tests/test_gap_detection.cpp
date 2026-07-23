#include <gtest/gtest.h>
#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"
#include <fcntl.h>
#if defined(_WIN32)
#include <io.h>
#define OPEN_FUNC _open
#define CLOSE_FUNC _close
#define DEV_NULL "NUL"
#define O_WRONLY_FLAG _O_WRONLY
#define O_APPEND_FLAG _O_APPEND
#else
#include <unistd.h>
#define OPEN_FUNC open
#define CLOSE_FUNC close
#define DEV_NULL "/dev/null"
#define O_WRONLY_FLAG O_WRONLY
#define O_APPEND_FLAG O_APPEND
#endif
#include <cstring>

class GapDetectionTest : public ::testing::Test {
protected:
    egbos::shm::MarketEventsRegion market_events;
    egbos::shm::GovernanceFlags gov_flags;
    int tier0_fd;

    void SetUp() override {
        // S2-REQ-002 requires O_APPEND on tier0
        // We use DEV_NULL for tests
        tier0_fd = OPEN_FUNC(DEV_NULL, O_WRONLY_FLAG | O_APPEND_FLAG);
        ASSERT_GE(tier0_fd, 0);
    }
    
    void TearDown() override {
        if (tier0_fd >= 0) CLOSE_FUNC(tier0_fd);
    }
};

TEST_F(GapDetectionTest, DetectsGapAndEmitsEvent) {
    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, tier0_fd);
    
    // Normal sequence (1)
    uint8_t payload1[16] = {0};
    uint64_t seq = 1;
    std::memcpy(payload1, &seq, sizeof(seq));
    ingestor.process_payload(payload1, 16);
    
    // Gap sequence (3) - Should emit GapEvent (S2-REQ-003)
    uint8_t payload3[16] = {0};
    seq = 3;
    std::memcpy(payload3, &seq, sizeof(seq));
    ingestor.process_payload(payload3, 16);
    
    egbos::shm::MarketEventSlot slot;
    
    // 1. Seq 1
    ASSERT_TRUE(market_events.pop(slot));
    
    // 2. Gap Event
    ASSERT_TRUE(market_events.pop(slot));
    EXPECT_EQ(slot.data[0], 0xFF);
    EXPECT_EQ(slot.data[1], 1); // GAP marker
    
    // 3. Seq 3
    ASSERT_TRUE(market_events.pop(slot));
}

TEST_F(GapDetectionTest, DetectsStalenessAndEmitsEvent) {
    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, tier0_fd);
    
    // Must initialize last_recv_time_ns_ internally first
    ingestor.check_staleness(0);
    
    // Mock internal tracker by processing a payload
    uint8_t payload[16] = {0};
    uint64_t seq = 1;
    std::memcpy(payload, &seq, sizeof(seq));
    ingestor.process_payload(payload, 16);
    
    // Fast forward past 500ms
    uint64_t future_time = 600000000ULL; // 600ms
    ingestor.check_staleness(future_time);
    
    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events.pop(slot)); // Normal payload
    
    ASSERT_TRUE(market_events.pop(slot)); // Stale Event (S2-REQ-004)
    EXPECT_EQ(slot.data[0], 0xFF);
    EXPECT_EQ(slot.data[1], 2); // STALE marker
}
