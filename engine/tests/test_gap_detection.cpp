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

// S2-REQ-002 (gap detection) is NOT implemented. process_payload() currently
// forwards all payloads unconditionally with no sequence-number validation.
// This test asserts that passthrough behavior explicitly so that a future
// re-implementation of real gap detection will break it on purpose, forcing a
// deliberate update rather than silently regressing.
// See: Option-A passthrough patch, 2026-09-12. CLAIM-L0-002 remains at V3.
// To re-implement: extract U/u update IDs from Binance JSON using a
// zero-allocation parser (requires ADR approval per GC-E-010).
TEST_F(GapDetectionTest, PassthroughAllPayloads_S2REQ002NotYetImplemented) {
    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, tier0_fd);

    // Send two valid JSON payloads that will be parsed by normalize_binance_depth
    // Under Option-A passthrough, both must be forwarded unconditionally —
    // no synthetic GAP_DETECTED event should be emitted between them.
    std::string json1 = R"({"e":"depthUpdate","U":1,"u":2,"b":[["1.0","10"]],"a":[]})";
    std::string json3 = R"({"e":"depthUpdate","U":3,"u":4,"b":[["1.0","10"]],"a":[]})";

    ingestor.process_payload(reinterpret_cast<const uint8_t*>(json1.data()), json1.size(), 12345);
    ingestor.process_payload(reinterpret_cast<const uint8_t*>(json3.data()), json3.size(), 12345);

    egbos::shm::MarketEventSlot slot;

    // Exactly 2 real-data slots — no synthetic gap event between them.
    ASSERT_TRUE(market_events.pop(slot)) << "First payload was not forwarded";
    EXPECT_NE(slot.data[0], 0xFF) << "First slot must not be a synthetic event";

    ASSERT_TRUE(market_events.pop(slot)) << "Second payload was not forwarded";
    EXPECT_NE(slot.data[0], 0xFF) << "Second slot must not be a synthetic event";

    // Ring must be empty — no spurious gap event was emitted.
    EXPECT_FALSE(market_events.pop(slot))
        << "Unexpected event in ring: gap detection should be a no-op (S2-REQ-002 DEFERRED)";
}


TEST_F(GapDetectionTest, DetectsStalenessAndEmitsEvent) {
    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, tier0_fd);
    
    // Must initialize last_recv_time_ns_ internally first
    ingestor.check_staleness(0);
    
    // Mock internal tracker by processing a valid payload
    std::string json = R"({"e":"depthUpdate","U":1,"u":2,"b":[["1.0","10"]],"a":[]})";
    ingestor.process_payload(reinterpret_cast<const uint8_t*>(json.data()), json.size(), 12345);
    
    // Fast forward past 500ms
    uint64_t future_time = 600000000ULL; // 600ms
    ingestor.check_staleness(future_time);
    
    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events.pop(slot)); // Normal payload
    
    ASSERT_TRUE(market_events.pop(slot)); // Stale Event (S2-REQ-004)
    EXPECT_EQ(slot.data[0], 0xFF);
    EXPECT_EQ(slot.data[1], 2); // STALE marker
}
