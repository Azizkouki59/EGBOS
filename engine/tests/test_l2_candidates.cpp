// engine/tests/test_l2_candidates.cpp
// EGBOS — Unit tests for L2 quantity-only computable candidates and
//         CKS OFI pure formula, including the verified counterexample
//         from verified research counterexample; NOT EGBOS-SPECIFIED for production use.
//
// GOVERNANCE NOTICE:
//   These tests exercise COMPUTABLE CANDIDATE functions that are NOT
//   EGBOS-approved production features. The purpose is to verify that
//   the math in the candidate module is correct so that when EGBOS
//   approves a feature contract for any of these functions, the approved
//   formula is already tested.
//
//   None of these tests constitute evidence that a feature is production-ready.
//   No feature_id has been assigned to any function under test.
//
// Test coverage:
//   1. QUEUE_IMBALANCE_1 — symmetry, all-bid, all-ask, denominator-zero
//   2. DEPTH_BID_5 / DEPTH_ASK_5 — accumulation, zero
//   3. DEPTH_IMBALANCE_5 — denominator-zero, balanced, all-bid
//   4. Per-level imbalance vector — happy path + mixed INVALID
//   5. Cumulative depth-imbalance curve — monotone accumulation
//   6. Herfindahl concentration (NON-CANONICAL) — uniform, all-at-one
//   7. Near-touch depth share — K=1, K=3, K=5, invalid K, zero denominator
//   8. CKS bid_contribution — all three price-comparison branches
//   9. CKS ask_contribution — all three price-comparison branches
//  10. CKS OFI counterexample §5.4: OFI_A=0, OFI_B=40 (exact integers)

#include "engine/l2/candidates/candidates_common.h"
#include "engine/l2/candidates/qty_features.h"
#include "engine/l2/candidates/cks_ofi.h"

#include <gtest/gtest.h>
#include <cmath>

using namespace egbos::l2::candidates;
using namespace egbos::l2::candidates::cks;

// ─────────────────────────────────────────────────────────────────────────────
// 1. QUEUE_IMBALANCE_1
// ─────────────────────────────────────────────────────────────────────────────

TEST(QueueImbalance1, Balanced) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    v.bid_qtys[0] = 50;
    v.ask_qtys[0] = 50;
    auto r = queue_imbalance_1_candidate(v);
    ASSERT_EQ(r.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.value, 0.0);
}

TEST(QueueImbalance1, AllBid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    v.bid_qtys[0] = 100;
    v.ask_qtys[0] = 0;
    auto r = queue_imbalance_1_candidate(v);
    ASSERT_EQ(r.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.value, 1.0);
}

TEST(QueueImbalance1, AllAsk) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    v.bid_qtys[0] = 0;
    v.ask_qtys[0] = 100;
    auto r = queue_imbalance_1_candidate(v);
    ASSERT_EQ(r.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.value, -1.0);
}

TEST(QueueImbalance1, Asymmetric) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    v.bid_qtys[0] = 75;
    v.ask_qtys[0] = 25;
    auto r = queue_imbalance_1_candidate(v);
    ASSERT_EQ(r.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.value, 0.5);
}

TEST(QueueImbalance1, DenominatorZeroReturnsInvalid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // Denominator-zero policy is UNRESOLVED — see current unresolved governance decision §DENOM-ZERO.
    // This test asserts that the function signals INVALID rather than
    // silently returning zero or crashing.
    QtyView v{};  // all zeros
    auto r = queue_imbalance_1_candidate(v);
    EXPECT_EQ(r.status, ComputeStatus::INVALID);
}

TEST(QueueImbalance1, SymmetryProperty) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // imbalance(bid=A, ask=B) == -imbalance(bid=B, ask=A)
    QtyView v1{}, v2{};
    v1.bid_qtys[0] = 30;  v1.ask_qtys[0] = 70;
    v2.bid_qtys[0] = 70;  v2.ask_qtys[0] = 30;
    auto r1 = queue_imbalance_1_candidate(v1);
    auto r2 = queue_imbalance_1_candidate(v2);
    ASSERT_EQ(r1.status, ComputeStatus::OK);
    ASSERT_EQ(r2.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r1.value, -r2.value);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. DEPTH_BID_5 / DEPTH_ASK_5
// ─────────────────────────────────────────────────────────────────────────────

TEST(DepthBid5, AccumulatesAllLevels) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    for (unsigned i = 0; i < kLevels; ++i) {
        v.bid_qtys[i] = 10u * (i + 1u);  // 10, 20, 30, 40, 50
    }
    EXPECT_EQ(depth_bid_5_candidate(v), 150u);
}

TEST(DepthBid5, AllZero) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    EXPECT_EQ(depth_bid_5_candidate(v), 0u);
}

TEST(DepthAsk5, AccumulatesAllLevels) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    for (unsigned i = 0; i < kLevels; ++i) {
        v.ask_qtys[i] = 5u * (i + 1u);  // 5, 10, 15, 20, 25
    }
    EXPECT_EQ(depth_ask_5_candidate(v), 75u);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. DEPTH_IMBALANCE_5
// ─────────────────────────────────────────────────────────────────────────────

TEST(DepthImbalance5, Balanced) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    for (unsigned i = 0; i < kLevels; ++i) {
        v.bid_qtys[i] = 20;
        v.ask_qtys[i] = 20;
    }
    auto r = depth_imbalance_5_candidate(v);
    ASSERT_EQ(r.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.value, 0.0);
}

TEST(DepthImbalance5, AllBidSide) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    for (unsigned i = 0; i < kLevels; ++i) {
        v.bid_qtys[i] = 10;
        v.ask_qtys[i] = 0;
    }
    auto r = depth_imbalance_5_candidate(v);
    ASSERT_EQ(r.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.value, 1.0);
}

TEST(DepthImbalance5, DenominatorZeroReturnsInvalid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};  // all zeros
    auto r = depth_imbalance_5_candidate(v);
    EXPECT_EQ(r.status, ComputeStatus::INVALID);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Per-level imbalance vector
// ─────────────────────────────────────────────────────────────────────────────

TEST(PerLevelImbalance, AllLevelsValid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    for (unsigned i = 0; i < kLevels; ++i) {
        v.bid_qtys[i] = 60;
        v.ask_qtys[i] = 40;
    }
    auto result = per_level_imbalance_candidate(v);
    for (unsigned i = 0; i < kLevels; ++i) {
        ASSERT_EQ(result[i].status, ComputeStatus::OK) << "level " << i;
        EXPECT_DOUBLE_EQ(result[i].value, 0.2) << "level " << i;
    }
}

TEST(PerLevelImbalance, SomeLevelsInvalid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // Level 2 has both sides zero → must be INVALID
    QtyView v{};
    v.bid_qtys[0] = 10; v.ask_qtys[0] = 10;
    v.bid_qtys[1] = 20; v.ask_qtys[1] = 5;
    // level 2: both zero
    v.bid_qtys[3] = 5;  v.ask_qtys[3] = 15;
    v.bid_qtys[4] = 0;  v.ask_qtys[4] = 0;

    auto result = per_level_imbalance_candidate(v);
    EXPECT_EQ(result[0].status, ComputeStatus::OK);
    EXPECT_EQ(result[1].status, ComputeStatus::OK);
    EXPECT_EQ(result[2].status, ComputeStatus::INVALID);  // both zero
    EXPECT_EQ(result[3].status, ComputeStatus::OK);
    EXPECT_EQ(result[4].status, ComputeStatus::INVALID);  // both zero
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Cumulative depth-imbalance curve
// ─────────────────────────────────────────────────────────────────────────────

TEST(CumulativeDepthImbalance, MonotoneAccumulation) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // All levels have bid=60, ask=40 → imbalance at every prefix = 0.2
    QtyView v{};
    for (unsigned i = 0; i < kLevels; ++i) {
        v.bid_qtys[i] = 60;
        v.ask_qtys[i] = 40;
    }
    auto result = cumulative_depth_imbalance_candidate(v);
    for (unsigned k = 0; k < kLevels; ++k) {
        ASSERT_EQ(result[k].status, ComputeStatus::OK) << "prefix k=" << k+1;
        EXPECT_DOUBLE_EQ(result[k].value, 0.2) << "prefix k=" << k+1;
    }
}

TEST(CumulativeDepthImbalance, FirstLevelZeroIsInvalid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    // Level 0 = 0,0 → prefix-1 INVALID; then levels 1..4 have qty
    v.bid_qtys[1] = 10; v.ask_qtys[1] = 10;
    auto result = cumulative_depth_imbalance_candidate(v);
    EXPECT_EQ(result[0].status, ComputeStatus::INVALID);
    EXPECT_EQ(result[1].status, ComputeStatus::OK);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Herfindahl concentration (NON-CANONICAL)
// ─────────────────────────────────────────────────────────────────────────────

TEST(HerfindahlConcentration, Uniform) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // NON-CANONICAL — see research-only quantitative reference — NOT EGBOS-SPECIFIED
    QtyView v{};
    for (unsigned i = 0; i < kLevels; ++i) {
        v.bid_qtys[i] = 20;
        v.ask_qtys[i] = 20;
    }
    auto r = herfindahl_concentration_candidate(v);
    ASSERT_EQ(r.bid.status, ComputeStatus::OK);
    ASSERT_EQ(r.ask.status, ComputeStatus::OK);
    EXPECT_NEAR(r.bid.value, 1.0 / 5.0, 1e-12);
    EXPECT_NEAR(r.ask.value, 1.0 / 5.0, 1e-12);
}

TEST(HerfindahlConcentration, AllAtOneLevelBid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // NON-CANONICAL — see research-only quantitative reference — NOT EGBOS-SPECIFIED
    QtyView v{};
    v.bid_qtys[0] = 100;  // all depth at level 0
    v.ask_qtys[0] = 100;
    auto r = herfindahl_concentration_candidate(v);
    ASSERT_EQ(r.bid.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.bid.value, 1.0);
}

TEST(HerfindahlConcentration, ZeroTotalBidInvalid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // NON-CANONICAL — see research-only quantitative reference — NOT EGBOS-SPECIFIED
    QtyView v{};
    auto r = herfindahl_concentration_candidate(v);
    EXPECT_EQ(r.bid.status, ComputeStatus::INVALID);
    EXPECT_EQ(r.ask.status, ComputeStatus::INVALID);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. Near-touch depth share
// ─────────────────────────────────────────────────────────────────────────────

TEST(NearTouchDepthShare, K1IsLevel0Share) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // K is provided by caller — no default exists (current unresolved governance decision §NEAR-TOUCH-K)
    QtyView v{};
    v.bid_qtys[0] = 30; v.ask_qtys[0] = 20;
    v.bid_qtys[1] = 70; v.ask_qtys[1] = 30;
    // DEPTH_BID_5 = 100, DEPTH_ASK_5 = 50
    // near_touch_bid(K=1) = 30/100 = 0.3
    // near_touch_ask(K=1) = 20/50  = 0.4
    auto r = near_touch_depth_share_candidate(v, 1);
    ASSERT_EQ(r.bid.status, ComputeStatus::OK);
    ASSERT_EQ(r.ask.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.bid.value, 0.3);
    EXPECT_DOUBLE_EQ(r.ask.value, 0.4);
}

TEST(NearTouchDepthShare, K5IsAlwaysOne) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    for (unsigned i = 0; i < kLevels; ++i) {
        v.bid_qtys[i] = 10 * (i + 1u);
        v.ask_qtys[i] = 5 * (i + 1u);
    }
    auto r = near_touch_depth_share_candidate(v, 5);
    ASSERT_EQ(r.bid.status, ComputeStatus::OK);
    ASSERT_EQ(r.ask.status, ComputeStatus::OK);
    EXPECT_DOUBLE_EQ(r.bid.value, 1.0);
    EXPECT_DOUBLE_EQ(r.ask.value, 1.0);
}

TEST(NearTouchDepthShare, InvalidKZeroReturnsInvalid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    v.bid_qtys[0] = 10; v.ask_qtys[0] = 10;
    auto r = near_touch_depth_share_candidate(v, 0);
    EXPECT_EQ(r.bid.status, ComputeStatus::INVALID);
    EXPECT_EQ(r.ask.status, ComputeStatus::INVALID);
}

TEST(NearTouchDepthShare, InvalidKTooLargeReturnsInvalid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};
    v.bid_qtys[0] = 10; v.ask_qtys[0] = 10;
    auto r = near_touch_depth_share_candidate(v, 6);  // > kLevels=5
    EXPECT_EQ(r.bid.status, ComputeStatus::INVALID);
    EXPECT_EQ(r.ask.status, ComputeStatus::INVALID);
}

TEST(NearTouchDepthShare, ZeroDenominatorReturnsInvalid) {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    QtyView v{};  // all zeros
    auto r = near_touch_depth_share_candidate(v, 2);
    EXPECT_EQ(r.bid.status, ComputeStatus::INVALID);
    EXPECT_EQ(r.ask.status, ComputeStatus::INVALID);
}

// ─────────────────────────────────────────────────────────────────────────────
// 8. CKS bid_contribution — all three branches
// ─────────────────────────────────────────────────────────────────────────────

TEST(CksBidContribution, PriceRises) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    BestQuoteState prev{100, 10};
    BestQuoteState curr{101,  5};
    // P_n > P_{n-1}: e_n = +curr.qty = +5
    EXPECT_EQ(bid_contribution(prev, curr), 5);
}

TEST(CksBidContribution, PriceSameQtyIncreases) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    BestQuoteState prev{100, 10};
    BestQuoteState curr{100, 50};
    // P_n == P_{n-1}: e_n = curr.qty - prev.qty = 50 - 10 = +40
    EXPECT_EQ(bid_contribution(prev, curr), 40);
}

TEST(CksBidContribution, PriceSameQtyDecreases) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    BestQuoteState prev{100, 30};
    BestQuoteState curr{100, 10};
    // P_n == P_{n-1}: e_n = 10 - 30 = -20
    EXPECT_EQ(bid_contribution(prev, curr), -20);
}

TEST(CksBidContribution, PriceFalls) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    BestQuoteState prev{101, 5};
    BestQuoteState curr{100, 10};
    // P_n < P_{n-1}: e_n = -prev.qty = -5
    EXPECT_EQ(bid_contribution(prev, curr), -5);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. CKS ask_contribution — all three branches
// ─────────────────────────────────────────────────────────────────────────────

TEST(CksAskContribution, PriceFalls) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    BestQuoteState prev{101, 10};
    BestQuoteState curr{100,  5};
    // P_n < P_{n-1}: e_n = -curr.qty = -5
    EXPECT_EQ(ask_contribution(prev, curr), -5);
}

TEST(CksAskContribution, PriceSame) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    BestQuoteState prev{101, 20};
    BestQuoteState curr{101, 15};
    // P_n == P_{n-1}: e_n = prev.qty - curr.qty = 20 - 15 = +5
    EXPECT_EQ(ask_contribution(prev, curr), 5);
}

TEST(CksAskContribution, PriceRises) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    BestQuoteState prev{100, 10};
    BestQuoteState curr{101,  5};
    // P_n > P_{n-1}: e_n = +prev.qty = +10
    EXPECT_EQ(ask_contribution(prev, curr), 10);
}

// ─────────────────────────────────────────────────────────────────────────────
// 10. CKS OFI counterexample (verified research counterexample; NOT EGBOS-SPECIFIED for production use)
//     Expected: OFI_A = 0, OFI_B = 40 (exact integer arithmetic)
// ─────────────────────────────────────────────────────────────────────────────

// Fixture: initial state used in both paths.
// bid: price=100, qty=10  |  ask: price=101, qty=10
namespace {
constexpr BestQuoteState kBidState0  {100, 10};
constexpr BestQuoteState kAskState0  {101, 10};
// Final state (same as State 0 for both paths):
constexpr BestQuoteState kBidStateFinal {100, 10};
constexpr BestQuoteState kAskStateFinal {101, 10};
}

TEST(CksOfiCounterexample, PathA_OFI_is_zero) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    //
    // Path A (verified research counterexample; NOT EGBOS-SPECIFIED for production use): no intermediate events.
    // State 0 == State 1 == (bid: 100,10 | ask: 101,10).
    // bid_contribution(prev=kBidState0, curr=kBidStateFinal)
    //   = same price, same qty → 10 - 10 = 0
    // ask_contribution is symmetric → 0
    // OFI_A = 0 - 0 = 0.
    const int64_t ofi_a = ofi_contribution(
        kBidState0, kBidStateFinal,
        kAskState0, kAskStateFinal);
    EXPECT_EQ(ofi_a, 0) << "OFI_A must be 0 (spec §5.4 Path A)";
}

TEST(CksOfiCounterexample, PathB_OFI_is_forty) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    //
    // Path B (verified research counterexample; NOT EGBOS-SPECIFIED for production use): three intermediate events, bid side only.
    // Ask side is unchanged throughout (State 0 == State final for ask).
    //
    // Event 1: (100,10) → (100,50)
    //   bid_contribution: same price, qty 10→50 = +40
    //   ask_contribution: ask unchanged = 0 (prev==curr for ask)
    //   OFI contribution: +40
    //
    // Event 2: (100,50) → (101,5)
    //   bid_contribution: price rises → +curr.qty = +5
    //   ask_contribution: 0
    //   OFI contribution: +5
    //
    // Event 3: (101,5) → (100,10)
    //   bid_contribution: price falls → -prev.qty = -5
    //   ask_contribution: 0
    //   OFI contribution: -5
    //
    // OFI_B = 40 + 5 + (-5) = 40.

    // Constant ask (unchanged through all events)
    constexpr BestQuoteState kAskStatic {101, 10};

    // Event 1
    const int64_t e1 = ofi_contribution(
        {100, 10}, {100, 50},
        kAskStatic, kAskStatic);
    EXPECT_EQ(e1, 40) << "Event 1 contribution (spec §5.4 Path B)";

    // Event 2
    const int64_t e2 = ofi_contribution(
        {100, 50}, {101, 5},
        kAskStatic, kAskStatic);
    EXPECT_EQ(e2, 5) << "Event 2 contribution (spec §5.4 Path B)";

    // Event 3
    const int64_t e3 = ofi_contribution(
        {101, 5}, {100, 10},
        kAskStatic, kAskStatic);
    EXPECT_EQ(e3, -5) << "Event 3 contribution (spec §5.4 Path B)";

    // Aggregate OFI_B = e1 + e2 + e3
    const int64_t ofi_b = e1 + e2 + e3;
    EXPECT_EQ(ofi_b, 40) << "OFI_B must be 40 (spec §5.4 Path B)";
}

TEST(CksOfiCounterexample, PathAVsPathBAreDifferent) {
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    //
    // This test makes the indeterminacy explicit: same endpoints, different OFI.
    // This is the proof that SnapshotDeltaOFI ≠ OFI (verified research counterexample; NOT EGBOS-SPECIFIED for production use).
    constexpr BestQuoteState kAskStatic {101, 10};
    const int64_t ofi_a = ofi_contribution(kBidState0, kBidStateFinal,
                                           kAskState0, kAskStateFinal);
    const int64_t e1 = ofi_contribution({100,10},{100,50},kAskStatic,kAskStatic);
    const int64_t e2 = ofi_contribution({100,50},{101, 5},kAskStatic,kAskStatic);
    const int64_t e3 = ofi_contribution({101, 5},{100,10},kAskStatic,kAskStatic);
    const int64_t ofi_b = e1 + e2 + e3;

    EXPECT_EQ(ofi_a, 0);
    EXPECT_EQ(ofi_b, 40);
    EXPECT_NE(ofi_a, ofi_b)
        << "OFI_A != OFI_B despite identical endpoints — "
           "snapshot-pair OFI is indeterminate (spec §5.4). "
           "SnapshotDeltaOFI must never be named OFI or OFI_1s.";
}
