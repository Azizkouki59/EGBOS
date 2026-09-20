// engine/tests/test_lob_upsert.cpp
// EGBOS — LobEngine::upsert_synthetic_level() isolation tests
//
// Tests 1-4: verify the three internal routing paths of upsert_synthetic_level
//            (add path, modify path, cancel path, cancel-then-recreate path).
// Test 5:    isolates the Finding 21 fix — modify_order() calling update_bbo()
//            when total_qty drops to zero.
//
// Classification : LOCAL_SYNTHETIC / DEVELOPMENT_TEST
// Provenance     : NOT LIVE_OPERATIONAL
// Constraints    : No network, no SHM. All state is stack-local.

#include <gtest/gtest.h>
#include "engine/l1/lob_engine.h"

#include <cstdint>
#include <memory>

using namespace egbos::l1;

namespace {

// Convenience: anchor at 1,000,000,000 (10^9 in fixed-point kPriceScale=10^7
// units, i.e. $100.00), tick_size = 10,000,000 ($1.00 per tick).
// price_to_index(price) = kMaxPriceLevels/2 + (price - anchor)/tick.
// With anchor=1'000'000'000 and tick=10'000'000:
//   price 1'000'000'000 -> index 32768 (mid)
//   price 1'010'000'000 -> index 32769 (+1 tick)
//   price 1'020'000'000 -> index 32770 (+2 ticks)
//   price   990'000'000 -> index 32767 (-1 tick)
constexpr int64_t kAnchor = 1'000'000'000LL;
constexpr int64_t kTick   =    10'000'000LL;

constexpr int64_t kBidPrice = 990'000'000LL;  // index 32767 (below anchor)
constexpr int64_t kAskPrice = 1'010'000'000LL; // index 32769 (above anchor)

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — First call for a given price: add_order path
// After a single upsert with qty>0 for a price that has never been seen,
// generate_top5_snapshot() must reflect the level with the exact qty.
// ─────────────────────────────────────────────────────────────────────────────
TEST(LobUpsertTest, FirstCall_TakesAddOrderPath) {
    auto lob_ptr = std::make_unique<LobEngine>(kAnchor, kTick);
    LobEngine& lob = *lob_ptr;

    lob.upsert_synthetic_level(kBidPrice, 500, /*is_bid=*/true);

    Top5BidsAsks snap = lob.generate_top5_snapshot();

    // Best (and only) bid should be at kBidPrice with qty 500.
    EXPECT_EQ(snap.bid_qtys[0], 500);
    // No ask was added.
    EXPECT_EQ(snap.ask_qtys[0], 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — Second call, same price, different qty: modify_order path
// qty must reflect ONLY the latest value, not a cumulative sum.
// A cumulative sum would mean a second node was created (add path taken twice).
// ─────────────────────────────────────────────────────────────────────────────
TEST(LobUpsertTest, SecondCall_SamePrice_TakesModifyPath_NotAddPath) {
    auto lob_ptr = std::make_unique<LobEngine>(kAnchor, kTick);
    LobEngine& lob = *lob_ptr;

    lob.upsert_synthetic_level(kBidPrice, 500, /*is_bid=*/true);
    lob.upsert_synthetic_level(kBidPrice, 200, /*is_bid=*/true); // modify, not add

    Top5BidsAsks snap = lob.generate_top5_snapshot();

    // qty must be 200, NOT 700 (which would mean two nodes were created).
    EXPECT_EQ(snap.bid_qtys[0], 200);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — qty=0 call: cancel_order path
// After a qty=0 upsert, generate_top5_snapshot() must not show the level.
// ─────────────────────────────────────────────────────────────────────────────
TEST(LobUpsertTest, ZeroQty_TakesCancelPath) {
    auto lob_ptr = std::make_unique<LobEngine>(kAnchor, kTick);
    LobEngine& lob = *lob_ptr;

    lob.upsert_synthetic_level(kBidPrice, 500, /*is_bid=*/true);
    // Cancel by passing qty=0.
    lob.upsert_synthetic_level(kBidPrice, 0, /*is_bid=*/true);

    Top5BidsAsks snap = lob.generate_top5_snapshot();

    // Level must be gone from snapshot.
    EXPECT_EQ(snap.bid_qtys[0], 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — Cancel-then-recreate: after qty=0 (cancel_order), a subsequent
// non-zero call for the SAME price must take the add_order path again —
// NOT silently no-op via modify_order on a stale/nonexistent entry.
//
// This is the direct code proof of the "falls through to add_order" claim
// from the design note (Gap 2). If the probe incorrectly still found the
// old node, the call would route to modify_order, which would silently
// fail (node == nullptr check on line 96 of original lob_engine.cpp).
// The snapshot qty would then remain 0 instead of the new value.
// ─────────────────────────────────────────────────────────────────────────────
TEST(LobUpsertTest, CancelThenRecreate_TakesAddPathAgain) {
    auto lob_ptr = std::make_unique<LobEngine>(kAnchor, kTick);
    LobEngine& lob = *lob_ptr;

    // Step 1: create the level.
    lob.upsert_synthetic_level(kBidPrice, 500, /*is_bid=*/true);

    // Step 2: cancel (qty=0).
    lob.upsert_synthetic_level(kBidPrice, 0, /*is_bid=*/true);
    {
        Top5BidsAsks snap = lob.generate_top5_snapshot();
        ASSERT_EQ(snap.bid_qtys[0], 0) << "Precondition: level must be absent after cancel";
    }

    // Step 3: recreate at the same price with a new qty.
    // This MUST take the add_order path (not silently fail via modify_order).
    lob.upsert_synthetic_level(kBidPrice, 300, /*is_bid=*/true);

    Top5BidsAsks snap = lob.generate_top5_snapshot();

    // If qty is 300, add_order was taken correctly.
    // If qty is 0, the node was not found and modify_order silently no-op'd —
    // proving the probe correctly re-inserts rather than failing silently.
    EXPECT_EQ(snap.bid_qtys[0], 300)
        << "Re-creation after cancel must produce qty=300 via the add_order path. "
        << "qty=0 would indicate the modify_order silent-noop bug is present.";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5a — Finding 21 fix via upsert path:
// A BBO-level driven to qty=0 via upsert (which internally routes to
// cancel_order) must update best_bid_index_. Verified indirectly via the
// snapshot: after adding a second bid at a lower price, then cancelling
// the BBO bid, the snapshot must promote the second bid to position 0.
// ─────────────────────────────────────────────────────────────────────────────
TEST(LobUpsertTest, Finding21_BboUpdatesOnCancelViaupsert) {
    auto lob_ptr = std::make_unique<LobEngine>(kAnchor, kTick);
    LobEngine& lob = *lob_ptr;

    // Two bids: kBidPrice (index 32767, best) and one tick lower (index 32766).
    constexpr int64_t kLowerBid = kBidPrice - kTick; // 980,000,000
    lob.upsert_synthetic_level(kBidPrice,  500, true);  // best bid
    lob.upsert_synthetic_level(kLowerBid,  100, true);  // second best

    // Cancel the best bid.
    lob.upsert_synthetic_level(kBidPrice, 0, true);

    Top5BidsAsks snap = lob.generate_top5_snapshot();

    // kLowerBid must now be the best bid (position 0).
    EXPECT_EQ(snap.bid_qtys[0], 100)
        << "After cancelling BBO via upsert, the next-best bid must be promoted.";
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5b — Finding 21 fix isolated via direct modify_order() call:
// Calls modify_order directly (bypassing upsert) to confirm update_bbo() is
// triggered when total_qty drops to zero on the BBO level. This isolates the
// Finding 21 fix from the upsert method.
//
// Setup: add an order at kAskPrice (which becomes best ask), then modify it
// to qty=0. best_ask_index_ must advance past that level.
// ─────────────────────────────────────────────────────────────────────────────
TEST(LobUpsertTest, Finding21_ModifyOrderDirectly_ZeroQty_UpdatesBbo) {
    auto lob_ptr = std::make_unique<LobEngine>(kAnchor, kTick);
    LobEngine& lob = *lob_ptr;

    // Add ask at kAskPrice and a fallback ask one tick above.
    constexpr int64_t kHigherAsk = kAskPrice + kTick; // 1,020,000,000
    lob.add_order(1, kAskPrice,  50, /*is_bid=*/false); // best ask (order ID 1)
    lob.add_order(2, kHigherAsk, 20, /*is_bid=*/false); // fallback ask

    // Verify initial snapshot: best ask should be kAskPrice with qty 50.
    {
        Top5BidsAsks snap = lob.generate_top5_snapshot();
        ASSERT_EQ(snap.ask_qtys[0], 50) << "Precondition: kAskPrice is best ask";
    }

    // Directly modify order 1 to qty=0. Finding 21 fix: this must trigger
    // update_bbo() so best_ask_index_ advances to kHigherAsk.
    lob.modify_order(1, 0);

    Top5BidsAsks snap = lob.generate_top5_snapshot();

    // kHigherAsk must now be at position 0 with qty 20.
    EXPECT_EQ(snap.ask_qtys[0], 20)
        << "modify_order(id, 0) must trigger update_bbo() when the BBO level "
        << "drops to zero. qty still 50 at position 0 would mean BBO was not updated.";
    // The zero-qty slot from order 1 must NOT appear in the snapshot.
    EXPECT_NE(snap.ask_qtys[1], 50)
        << "Zero-qty level from modified order must not appear in snapshot.";
}
TEST(LobPriceIndexTest, ExtremePricesClampToEdges) {
    auto e = std::make_unique<egbos::l1::LobEngine>(100000, 100);
    e->add_order(1, INT64_MAX, 5, true);              // far-high bid
    e->add_order(2, INT64_MIN, 5, false);             // far-low ask
    auto s = e->generate_top5_snapshot();
    EXPECT_EQ(s.bid_price_offsets[0], 32767);         // top edge, not wrapped
    EXPECT_EQ(s.ask_price_offsets[0], -32768);        // bottom edge
}

TEST(LobPriceIndexTest, LargeNonOverflowingDiffClampsNotWraps) {
    auto e = std::make_unique<egbos::l1::LobEngine>(100000, 100);
    // diff fits in int64 (no subtraction overflow) but ticks >> 2^32
    e->add_order(1, (INT64_MAX / 100) * 100, 5, true);
    auto s = e->generate_top5_snapshot();
    EXPECT_EQ(s.bid_price_offsets[0], 32767);
}

TEST(LobPriceIndexTest, NegativeAnchorNearMin) {
    auto e = std::make_unique<egbos::l1::LobEngine>(INT64_MIN + 1000, 100);
    e->add_order(1, INT64_MAX, 5, true);
    auto s = e->generate_top5_snapshot();
    EXPECT_EQ(s.bid_price_offsets[0], 32767);
}

TEST(LobPriceIndexTest, ExactBoundaries) {
    auto e = std::make_unique<egbos::l1::LobEngine>(100000, 10);
    // kMaxPriceLevels = 65536. Midpoint = 32768.
    // Max index is 65535. Top edge: anchor + (32767 * tick)
    e->add_order(1, 100000 + 32767 * 10, 5, true);
    // Overflow top: anchor + (32768 * tick) -> clamps to 65535
    e->add_order(2, 100000 + 32768 * 10, 5, true);
    // Min index is 0. Bottom edge: anchor - (32768 * tick)
    e->add_order(3, 100000 - 32768 * 10, 5, false);
    // Overflow bottom: anchor - (32769 * tick) -> clamps to 0
    e->add_order(4, 100000 - 32769 * 10, 5, false);

    auto s = e->generate_top5_snapshot();
    EXPECT_EQ(s.bid_price_offsets[0], 32767); // Clamped & max
    EXPECT_EQ(s.ask_price_offsets[0], -32768); // Clamped & min
}

TEST(LobPriceIndexDeathTest, ConstructorRejectsNonPositiveTickSize) {
    EXPECT_DEATH({
        auto e = std::make_unique<egbos::l1::LobEngine>(100000, 0);
    }, "EGBOS TERMINAL_HALT: LobEngine fatal error: tick_size must be positive");

    EXPECT_DEATH({
        auto e = std::make_unique<egbos::l1::LobEngine>(100000, -1);
    }, "EGBOS TERMINAL_HALT: LobEngine fatal error: tick_size must be positive");

    EXPECT_DEATH({
        auto e = std::make_unique<egbos::l1::LobEngine>(100000, INT64_MIN);
    }, "EGBOS TERMINAL_HALT: LobEngine fatal error: tick_size must be positive");
}