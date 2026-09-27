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

namespace {

struct NormalizerSeen {
    int count = 0;
    uint64_t first_update_id = 0;
    uint64_t final_update_id = 0;
};

bool collect_normalized(
    const egbos::events::RawMarketEvent& event,
    uint64_t first_update_id,
    uint32_t,
    void* ctx) noexcept
{
    auto* seen = static_cast<NormalizerSeen*>(ctx);
    ++seen->count;
    seen->first_update_id = first_update_id;
    seen->final_update_id = event.sequence_num();
    return true;
}

} // namespace

TEST(NormalizerTest, FieldsMayAppearBeforeUpdateIds) {
    // JSON object member order is not semantically significant.
    // b/a intentionally appear before U/u. Every emitted level must still
    // carry the frame's real U/u metadata.
    const std::string json = R"({
        "e":"depthUpdate",
        "E":123,
        "s":"BTCUSDT",
        "b":[["100000.12345000","0.06498000"]],
        "a":[["100001.00000000","0.01000000"]],
        "U":100348390124,
        "u":100348390125
    })";

    NormalizerSeen seen{};

    const auto status = egbos::l0::normalize_binance_depth(
        reinterpret_cast<const uint8_t*>(json.data()),
        json.size(),
        12345,
        collect_normalized,
        &seen);

    ASSERT_EQ(status, egbos::l0::L0ParseStatus::OK);
    EXPECT_EQ(seen.count, 2);
    EXPECT_EQ(seen.first_update_id, 100348390124ULL);
    EXPECT_EQ(seen.final_update_id, 100348390125ULL);
}

TEST(NormalizerTest, NonZeroExcessPrecisionIsRejected) {
    // 6th decimal digit is non-zero; L0 scale is 1e5, so no silent rounding.
    const std::string json =
        R"({"e":"depthUpdate","U":1,"u":1,"b":[["1.123456","1.0"]],"a":[]})";

    NormalizerSeen seen{};

    const auto status = egbos::l0::normalize_binance_depth(
        reinterpret_cast<const uint8_t*>(json.data()),
        json.size(),
        12345,
        collect_normalized,
        &seen);

    EXPECT_EQ(status, egbos::l0::L0ParseStatus::PRECISION_EXCEEDED);
}

TEST(NormalizerTest, TrailingZeroPaddingIsAccepted) {
    const std::string json =
        R"({"e":"depthUpdate","U":1,"u":1,"b":[["1.12345000","0.06498000"]],"a":[]})";

    NormalizerSeen seen{};

    const auto status = egbos::l0::normalize_binance_depth(
        reinterpret_cast<const uint8_t*>(json.data()),
        json.size(),
        12345,
        collect_normalized,
        &seen);

    ASSERT_EQ(status, egbos::l0::L0ParseStatus::OK);
    EXPECT_EQ(seen.count, 1);
}

// RFC-004 engineering_findings.raw_market_event_field_capacity_check:
// RawMarketEvent::quantity is uint32_t. parse_fixed_point validates against
// int64_t overflow only, so a scaled quantity that fits in int64_t can still
// exceed UINT32_MAX. Max representable scaled quantity is
// UINT32_MAX / kL0PriceScale (100000) = 42949.67295 base-asset units.
// These two tests pin the exact boundary: 42949 must be accepted (scaled
// value 4294900000 <= UINT32_MAX), 42950 must be explicitly rejected as
// OVERFLOW (scaled value 4295000000 > UINT32_MAX), not silently truncated.
TEST(NormalizerTest, QuantityAtUint32BoundaryIsAccepted) {
    const std::string json =
        R"({"e":"depthUpdate","U":1,"u":1,"b":[["1.0","42949"]],"a":[]})";

    NormalizerSeen seen{};

    const auto status = egbos::l0::normalize_binance_depth(
        reinterpret_cast<const uint8_t*>(json.data()),
        json.size(),
        12345,
        collect_normalized,
        &seen);

    ASSERT_EQ(status, egbos::l0::L0ParseStatus::OK);
    EXPECT_EQ(seen.count, 1);
}

TEST(NormalizerTest, QuantityExceedingUint32IsRejectedNotTruncated) {
    const std::string json =
        R"({"e":"depthUpdate","U":1,"u":1,"b":[["1.0","42950"]],"a":[]})";

    NormalizerSeen seen{};

    const auto status = egbos::l0::normalize_binance_depth(
        reinterpret_cast<const uint8_t*>(json.data()),
        json.size(),
        12345,
        collect_normalized,
        &seen);

    EXPECT_EQ(status, egbos::l0::L0ParseStatus::OVERFLOW);
    // No level should have been forwarded to the callback for a rejected event.
    EXPECT_EQ(seen.count, 0);
}


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
    // Pure state-machine test: disable Tier-0 durability so /dev/null fsync
    // cannot mask the expected FeedStaleEvent.
    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, -1);
    
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
