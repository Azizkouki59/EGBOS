// engine/tests/test_timestamp_injection.cpp
// Classification: LOCAL_SYNTHETIC
#include <gtest/gtest.h>
#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"
#include "schemas/generated/market_event_generated.h"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#if defined(_WIN32)
#define OPEN_FUNC _open
#define CLOSE_FUNC _close
#define O_FLAGS (_O_WRONLY | _O_APPEND | _O_BINARY)
#define DEV_NULL "NUL"
#else
#define OPEN_FUNC open
#define CLOSE_FUNC close
#define O_FLAGS (O_WRONLY | O_APPEND)
#define DEV_NULL "/dev/null"
#endif

class TimestampInjectionTest : public ::testing::Test {
protected:
    egbos::shm::MarketEventsRegion market_events;
    egbos::shm::GovernanceFlags gov_flags;
    int tier0_fd;

    void SetUp() override {
        std::memset(&market_events, 0, sizeof(market_events));
        std::memset(&gov_flags, 0, sizeof(gov_flags));
        tier0_fd = OPEN_FUNC(DEV_NULL, O_FLAGS);
        ASSERT_GE(tier0_fd, 0);
    }

    void TearDown() override {
        if (tier0_fd >= 0) CLOSE_FUNC(tier0_fd);
    }
};

TEST_F(TimestampInjectionTest, InjectTimestampAndVerifyABI) {
    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, tier0_fd);

    // Wire packet stub (8 bytes seq + 16 bytes payload)
    uint8_t payload[24] = {0};
    uint64_t seq = 1;
    std::memcpy(payload, &seq, sizeof(uint64_t));

    // The timestamp we want to inject
    uint64_t rx_timestamp_ns = 9876543210ULL;

    ingestor.process_payload(payload, sizeof(payload), rx_timestamp_ns);

    // Verify it was pushed to SPSC
    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events.pop(slot));

    // 1. ABI Check
    ASSERT_EQ(sizeof(egbos::shm::MarketEventSlot), 128);

    // 2. Timestamp reaches the field
    // Field ingestion_tsc_ is at offset 24 inside the struct
    uint64_t extracted_ts;
    std::memcpy(&extracted_ts, slot.data + 24, sizeof(uint64_t));

    // Reverse the EndianScalar if needed, but assuming little endian for this test
    ASSERT_EQ(extracted_ts, rx_timestamp_ns);
}

// 7. Stale detection uses the injected timestamp
TEST_F(TimestampInjectionTest, StaleDetectionUsesInjectedTimestamp) {
    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, tier0_fd);

    uint8_t payload[24] = {0};
    uint64_t seq = 1;
    std::memcpy(payload, &seq, sizeof(uint64_t));

    uint64_t rx_timestamp_ns = 1000000000ULL; // 1 second
    ingestor.process_payload(payload, sizeof(payload), rx_timestamp_ns);

    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events.pop(slot)); // consume normal event

    // 1.5 seconds later, check staleness. Threshold is 500ms (500,000,000 ns).
    ingestor.check_staleness(1500000001ULL);

    // Should have emitted a stale event
    ASSERT_TRUE(market_events.pop(slot));
    ASSERT_EQ(slot.data[0], 0xFF); // Synthetic
    ASSERT_EQ(slot.data[1], 2);    // STALE
}
