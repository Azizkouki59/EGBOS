// engine/tests/test_l1_consumer.cpp
// EGBOS — L1Consumer unit tests
//
// S2-REQ-001 (F3): L1Consumer now reads RawMarketEvent structs from slots
// (placed there by FeedIngestor::normalize_and_forward).  Tests push
// RawMarketEvent-bearing slots directly into the ring.
//
// Classification : LOCAL_SYNTHETIC / DEVELOPMENT_TEST
// Provenance     : NOT LIVE_OPERATIONAL

#include <gtest/gtest.h>
#include "engine/l1/l1_consumer.h"
#include "engine/l1/lob_snapshot_publisher.h"
#include "engine/l2/lob_snapshot_reader.h"
#include "engine/l1/lob_engine.h"
#include "engine/core/shm_layout.h"
#include "market_event_generated.h"

#include <cstring>
#include <string>
#include <cinttypes>

using namespace egbos::l1;
using namespace egbos::shm;
using namespace egbos::events;

// ── Helpers ──────────────────────────────────────────────────────────────────

// Build a MarketEventSlot containing a RawMarketEvent at offset 0
// and a SlotFrameMetadata at offset 64.
static MarketEventSlot make_event_slot(
    uint64_t       first_u,
    uint64_t       final_u,
    int64_t        price        = 100'000,   // 1.0 at kPriceScale=10^5
    uint32_t       quantity     = 100'000,   // 1.0
    Side           side         = Side_BUY,
    EventType      event_type   = EventType_NEW,
    uint64_t       ingestion_tsc = 0)
{
    MarketEventSlot slot{};
    RawMarketEvent event(
        /*schema_version=*/ 1,
        /*event_type=*/     event_type,
        /*side=*/           side,
        /*feed_id=*/        0,
        /*exchange_id=*/    0,
        /*instrument_id=*/  0,
        /*quantity=*/       quantity,
        /*sequence_num=*/   final_u,
        /*ingestion_tsc=*/  ingestion_tsc,
        /*exchange_tsc=*/   0,
        /*price=*/          price,
        /*tier0_offset=*/   0,
        /*checksum=*/       0
    );
    static_assert(sizeof(RawMarketEvent) <= 64,
        "RawMarketEvent does not fit in lower half of MarketEventSlot");
    std::memcpy(slot.data, &event, sizeof(RawMarketEvent));

    SlotFrameMetadata& meta = slot_frame_meta(slot);
    meta.first_update_id = first_u;
    meta.final_update_id = final_u;
    meta.frame_level_index = 0;

    return slot;
}

static void push_event(MarketEventsRegion& ring,
                       uint64_t first_u, uint64_t final_u,
                       int64_t price = 100'000, uint32_t qty = 100'000,
                       Side side = Side_BUY)
{
    ring.push(make_event_slot(first_u, final_u, price, qty, side));
}

// ── Test fixture ──────────────────────────────────────────────────────────────
class L1ConsumerTest : public ::testing::Test {
protected:
    MarketEventsRegion ring{};
    GovernanceFlags    gov_flags{};
    std::unique_ptr<L1Consumer> consumer;

    void SetUp() override {
        consumer = std::make_unique<L1Consumer>(ring, gov_flags);
    }
};

// ── Test A: Same frame, multiple levels ──────────────────────────────────────
TEST_F(L1ConsumerTest, TestA_SameFrameMultipleLevels) {
    // Frame: U=100, u=102, 3 levels
    push_event(ring, 100, 102); // level 1
    push_event(ring, 100, 102); // level 2
    push_event(ring, 100, 102); // level 3

    uint64_t n = consumer->consume_available();

    EXPECT_EQ(n, 3);
    EXPECT_EQ(consumer->counts().ok, 3);
    EXPECT_EQ(consumer->counts().same_frame_levels, 2); // 2nd and 3rd level are same-frame
    EXPECT_EQ(consumer->counts().sequence_gap_detected, 0);
    EXPECT_FALSE(consumer->is_degraded());
}

// ── Test B: Genuine next-frame continuity ────────────────────────────────────
TEST_F(L1ConsumerTest, TestB_ValidNextFrameContinuity) {
    // Frame 1: U=100, u=102
    push_event(ring, 100, 102);
    // Frame 2: U=103, u=105
    push_event(ring, 103, 105);

    uint64_t n = consumer->consume_available();

    EXPECT_EQ(n, 2);
    EXPECT_EQ(consumer->counts().ok, 2);
    EXPECT_EQ(consumer->counts().same_frame_levels, 0);
    EXPECT_EQ(consumer->counts().sequence_gap_detected, 0);
    EXPECT_FALSE(consumer->is_degraded());
}

// ── Test C: Real gap ──────────────────────────────────────────────────────────
TEST_F(L1ConsumerTest, TestC_GenuineGap) {
    // Frame 1: U=100, u=102
    push_event(ring, 100, 102);
    // Frame 2: U=110, u=112 (gap, expected U=103)
    push_event(ring, 110, 112);
    
    // extra level in gapped frame should be skipped
    push_event(ring, 110, 112);

    consumer->consume_available();

    EXPECT_EQ(consumer->slots_popped(), 3);
    EXPECT_EQ(consumer->counts().ok, 1);
    EXPECT_EQ(consumer->counts().sequence_gap_detected, 1);
    EXPECT_EQ(consumer->counts().skipped_while_degraded, 1); // the 3rd event
    EXPECT_TRUE(consumer->is_degraded());
}

// ── Test D: Repeated same frame vs repeated new frame ────────────────────────
TEST_F(L1ConsumerTest, TestD_SameFrameVsNewFrameDuplicate) {
    // Frame 1: U=100, u=102
    push_event(ring, 100, 102);
    // Same frame again (e.g. multi-level) -> OK
    push_event(ring, 100, 102);

    consumer->consume_available();
    EXPECT_FALSE(consumer->is_degraded());
    EXPECT_EQ(consumer->counts().same_frame_levels, 1);

    // Now send a *new* frame that illegally repeats u=102 (but with different U)
    push_event(ring, 101, 102); // U=101 != prev u + 1 (103)

    consumer->consume_available();
    EXPECT_TRUE(consumer->is_degraded());
    EXPECT_EQ(consumer->counts().sequence_gap_detected, 1);
}

// ── Test 4: CANCEL event (qty=0) is processed OK ─────────────────────────────
TEST_F(L1ConsumerTest, CancelEvent_IsProcessedOK) {
    ring.push(make_event_slot(1, 1, 100'000, 0, Side_BUY, EventType_CANCEL));
    consumer->consume_available();
    EXPECT_FALSE(consumer->is_degraded());
}

// ── Test 6: All-zero slot (padding slot, seq=0) — first event always accepted
TEST_F(L1ConsumerTest, ZeroSlot_FirstEventAccepted) {
    MarketEventSlot slot{};
    ring.push(slot);
    consumer->consume_available();
    EXPECT_FALSE(consumer->is_degraded());
}

// ── Test 11: reset() clears all counters and gap state ───────────────────────
TEST_F(L1ConsumerTest, Reset_ClearsAllCountersAndGapState) {
    push_event(ring, 1, 1);
    push_event(ring, 9, 9); // gap
    consumer->consume_available();
    EXPECT_TRUE(consumer->is_degraded());

    consumer->reset();

    EXPECT_EQ(consumer->slots_popped(), 0);
    EXPECT_FALSE(consumer->is_degraded());
}

// ── Test 12: Large sequence of sequential events — no DEGRADED ───────────────
TEST_F(L1ConsumerTest, LargeSequentialRun_NoDegraded) {
    uint64_t last_u = 0;
    for (uint64_t u = 1; u <= 1000; ++u) {
        push_event(ring, last_u + 1, u);
        last_u = u;
    }
    uint64_t n = consumer->consume_available();

    EXPECT_EQ(n, 1000);
    EXPECT_FALSE(consumer->is_degraded());
}


// ── Synthetic STALE control event must not enter Binance U/u validation ─────
TEST_F(L1ConsumerTest, SyntheticStaleEvent_DoesNotCreateSequenceGap) {
    // First valid Binance frame.
    push_event(ring, 100, 100);

    // Exact encoding produced by FeedIngestor::emit_synthetic_event().
    MarketEventSlot stale{};
    stale.data[0] = 0xFF;
    stale.data[1] = 2;  // FEED_STALE
    const uint64_t timestamp = 123456789ULL;
    std::memcpy(&stale.data[2], &timestamp, sizeof(timestamp));
    ring.push(stale);

    // This frame would be the valid next frame, but L1 remains degraded
    // after the explicit STALE control event.
    push_event(ring, 101, 101);

    consumer->consume_available();

    EXPECT_TRUE(consumer->is_degraded());
    EXPECT_EQ(consumer->counts().sequence_gap_detected, 0);
    EXPECT_EQ(consumer->counts().ok, 1);
    EXPECT_EQ(consumer->counts().skipped_while_degraded, 1);
}


// ── L1 -> L2 snapshot publication integration ───────────────────────────────
TEST_F(L1ConsumerTest, PublishesL1SnapshotToL2) {
    auto region = std::make_unique<LobSnapshotsRegion>();

    LobSnapshotPublisher publisher(region.get());
    egbos::l2::LobSnapshotReader reader(region.get());

    auto lob = std::make_unique<LobEngine>(100'000LL, 100LL);
    L1Consumer integrated_consumer(
        ring,
        gov_flags,
        lob.get(),
        &publisher
    );

    push_event(ring, 1, 1, 100'000, 100'000, Side_BUY);

    ASSERT_EQ(integrated_consumer.consume_available(), 1);
    ASSERT_FALSE(integrated_consumer.is_degraded());

    Top5BidsAsks snapshot{};
    ASSERT_TRUE(reader.read(0, snapshot));

    EXPECT_EQ(snapshot.bid_qtys[0], 100'000u);
    EXPECT_GT(snapshot.bid_qtys[0], 0u);
}
