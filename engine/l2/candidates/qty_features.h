// engine/l2/candidates/qty_features.h
// EGBOS — L2 Candidate Feature Module: quantity-only computable candidates
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE (see
// research-only quantitative reference — NOT EGBOS-SPECIFIED).
//
// All functions in this file are computable from bid_qtys[] / ask_qtys[]
// alone, with NO price-reconstruction dependency. They are isolated here
// because they are the only §8.1 features that can be safely evaluated
// from the current Top5BidsAsks ABI.
//
// These functions MUST NOT be called from the production
// FeatureEngine::process_instrument() path. They exist so that when EGBOS
// approves a feature contract for any of them, the math is already correct
// and tested.
//
// Naming convention: every function name ends with _candidate to make the
// non-production status immediately visible at call sites.
//
// Source authority for each formula: research-only quantitative reference — NOT EGBOS-SPECIFIED (verbatim
// formulas reproduced in each function's doc comment below).
//
// OPEN DECISIONS referenced throughout:
//   §DENOM-ZERO   — denominator-zero policy (unresolved)
//   §FEAT-CONTRACT — no EGBOS feature contract exists for any function here

#pragma once

#include "engine/l2/candidates/candidates_common.h"
#include <cstdint>
#include <array>

namespace egbos::l2::candidates {

// ── Input view ────────────────────────────────────────────────────────────
//
// A non-owning view of the quantity fields from one Top5BidsAsks snapshot.
// Callers populate this from egbos::l1::Top5BidsAsks before passing to
// candidate functions. This decouples the candidate module from the ABI
// header and makes the Python parity layer easier to test without SHM.
struct QtyView {
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    uint32_t bid_qtys[kLevels];  // level 0 = best bid
    uint32_t ask_qtys[kLevels];  // level 0 = best ask
};


// ── QUEUE_IMBALANCE_1 ─────────────────────────────────────────────────────
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
//
// Canonical spec §4.1 / §8.1 verbatim formula:
//
//   QUEUE_IMBALANCE_1 = (bid_qtys[0] - ask_qtys[0])
//                       / (bid_qtys[0] + ask_qtys[0])
//
// Scope: best-bid vs. best-ask only (level 0).
// Range: (-1, 1) when denominator > 0.
//
// Denominator-zero policy: UNRESOLVED — see current unresolved governance decision §DENOM-ZERO.
// When bid_qtys[0] + ask_qtys[0] == 0, this function returns ComputeStatus::INVALID
// and leaves .value undefined. Callers MUST check .status before using .value.
//
// BLOCKED: see current unresolved governance decision §FEAT-CONTRACT, §DENOM-ZERO.
[[nodiscard]] inline Result<double>
queue_imbalance_1_candidate(const QtyView& v) noexcept
{
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
    const double bid0 = static_cast<double>(v.bid_qtys[0]);
    const double ask0 = static_cast<double>(v.ask_qtys[0]);
    const double denom = bid0 + ask0;
    if (denom == 0.0) {
        // BLOCKED: see current unresolved governance decision §DENOM-ZERO
        return {0.0, ComputeStatus::INVALID};
    }
    return {(bid0 - ask0) / denom, ComputeStatus::OK};
}


// ── DEPTH_BID_5 ───────────────────────────────────────────────────────────
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
//
// Canonical spec §4.2 / §8.1 verbatim formula:
//
//   DEPTH_BID_5 = sum(bid_qtys[0..4])
//
// Scope: top-5 bid side aggregate quantity.
// Always valid (sum of non-negative integers, no division).
[[nodiscard]] inline uint64_t
depth_bid_5_candidate(const QtyView& v) noexcept
{
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
    uint64_t total = 0u;
    for (unsigned i = 0; i < kLevels; ++i) {
        total += static_cast<uint64_t>(v.bid_qtys[i]);
    }
    return total;
}


// ── DEPTH_ASK_5 ───────────────────────────────────────────────────────────
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
//
// Canonical spec §4.3 / §8.1 verbatim formula:
//
//   DEPTH_ASK_5 = sum(ask_qtys[0..4])
//
// Scope: top-5 ask side aggregate quantity.
// Always valid (no division).
[[nodiscard]] inline uint64_t
depth_ask_5_candidate(const QtyView& v) noexcept
{
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
    uint64_t total = 0u;
    for (unsigned i = 0; i < kLevels; ++i) {
        total += static_cast<uint64_t>(v.ask_qtys[i]);
    }
    return total;
}


// ── DEPTH_IMBALANCE_5 ────────────────────────────────────────────────────
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
//
// Canonical spec §4.4 / §8.1 verbatim formula:
//
//   DEPTH_IMBALANCE_5 = (DEPTH_BID_5 - DEPTH_ASK_5)
//                       / (DEPTH_BID_5 + DEPTH_ASK_5)
//
// Denominator-zero policy: UNRESOLVED — see current unresolved governance decision §DENOM-ZERO.
// Returns INVALID when total depth on both sides is 0.
//
// BLOCKED: see current unresolved governance decision §FEAT-CONTRACT, §DENOM-ZERO.
[[nodiscard]] inline Result<double>
depth_imbalance_5_candidate(const QtyView& v) noexcept
{
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
    const double bid5 = static_cast<double>(depth_bid_5_candidate(v));
    const double ask5 = static_cast<double>(depth_ask_5_candidate(v));
    const double denom = bid5 + ask5;
    if (denom == 0.0) {
        // BLOCKED: see current unresolved governance decision §DENOM-ZERO
        return {0.0, ComputeStatus::INVALID};
    }
    return {(bid5 - ask5) / denom, ComputeStatus::OK};
}


// ── Per-level imbalance vector (§4.7) ────────────────────────────────────
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
//
// Canonical spec §4.7 verbatim formula (for each level i ∈ {0..4}):
//
//   imbalance[i] = (bid_qtys[i] - ask_qtys[i])
//                  / (bid_qtys[i] + ask_qtys[i])
//
// Returns a per-level Result array. Levels where both sides are zero are
// marked INVALID individually.
//
// Denominator-zero policy: UNRESOLVED — see current unresolved governance decision §DENOM-ZERO.
// BLOCKED: see current unresolved governance decision §FEAT-CONTRACT, §DENOM-ZERO.
[[nodiscard]] inline std::array<Result<double>, kLevels>
per_level_imbalance_candidate(const QtyView& v) noexcept
{
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
    std::array<Result<double>, kLevels> result{};
    for (unsigned i = 0; i < kLevels; ++i) {
        const double bid_i = static_cast<double>(v.bid_qtys[i]);
        const double ask_i = static_cast<double>(v.ask_qtys[i]);
        const double denom = bid_i + ask_i;
        if (denom == 0.0) {
            // BLOCKED: see current unresolved governance decision §DENOM-ZERO
            result[i] = {0.0, ComputeStatus::INVALID};
        } else {
            result[i] = {(bid_i - ask_i) / denom, ComputeStatus::OK};
        }
    }
    return result;
}


// ── Cumulative depth-imbalance curve (§4.8) ──────────────────────────────
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
//
// Canonical spec §4.8 verbatim formula (for each prefix depth k ∈ {1..5}):
//
//   cumulative_imbalance[k] = (sum_i=0^{k-1} bid_qtys[i]  -  sum_i=0^{k-1} ask_qtys[i])
//                             / (sum_i=0^{k-1} bid_qtys[i]  +  sum_i=0^{k-1} ask_qtys[i])
//
// Result index 0 = k=1 (best level only), index 4 = k=5 (all 5 levels).
//
// Denominator-zero policy: UNRESOLVED — see current unresolved governance decision §DENOM-ZERO.
// BLOCKED: see current unresolved governance decision §FEAT-CONTRACT, §DENOM-ZERO.
[[nodiscard]] inline std::array<Result<double>, kLevels>
cumulative_depth_imbalance_candidate(const QtyView& v) noexcept
{
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
    std::array<Result<double>, kLevels> result{};
    double cum_bid = 0.0;
    double cum_ask = 0.0;
    for (unsigned k = 0; k < kLevels; ++k) {
        cum_bid += static_cast<double>(v.bid_qtys[k]);
        cum_ask += static_cast<double>(v.ask_qtys[k]);
        const double denom = cum_bid + cum_ask;
        if (denom == 0.0) {
            // BLOCKED: see current unresolved governance decision §DENOM-ZERO
            result[k] = {0.0, ComputeStatus::INVALID};
        } else {
            result[k] = {(cum_bid - cum_ask) / denom, ComputeStatus::OK};
        }
    }
    return result;
}


// ── Depth concentration / Herfindahl (§4.9) ──────────────────────────────
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// NON-CANONICAL: remains labeled non-canonical in code and docs per the
// prompt instruction. See research-only quantitative reference — NOT EGBOS-SPECIFIED.
//
// Canonical spec §4.9 verbatim formula (bid side; ask side symmetric):
//
//   HHI_bid = sum_i (bid_qtys[i] / DEPTH_BID_5)^2
//
// The Herfindahl–Hirschman Index measures how concentrated the bid-side
// depth is across the 5 levels. HHI_bid = 1.0 means all depth at one level.
// HHI_bid = 1/5 = 0.2 means perfectly uniform across all 5 levels.
//
// NON-CANONICAL: no EGBOS adoption decision. Awaiting spec §4.9 resolution.
// Denominator-zero policy: UNRESOLVED — see current unresolved governance decision §DENOM-ZERO.
// BLOCKED: see current unresolved governance decision §FEAT-CONTRACT, §DENOM-ZERO.
struct HHIResult {
    Result<double> bid;  // HHI for bid side
    Result<double> ask;  // HHI for ask side
};

[[nodiscard]] inline HHIResult
herfindahl_concentration_candidate(const QtyView& v) noexcept
{
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // NON-CANONICAL — see research-only quantitative reference — NOT EGBOS-SPECIFIED
    // BLOCKED: see current unresolved governance decision §FEAT-CONTRACT, §DENOM-ZERO
    const double bid5 = static_cast<double>(depth_bid_5_candidate(v));
    const double ask5 = static_cast<double>(depth_ask_5_candidate(v));

    HHIResult out{};

    if (bid5 == 0.0) {
        // BLOCKED: see current unresolved governance decision §DENOM-ZERO
        out.bid = {0.0, ComputeStatus::INVALID};
    } else {
        double hhi = 0.0;
        for (unsigned i = 0; i < kLevels; ++i) {
            const double share = static_cast<double>(v.bid_qtys[i]) / bid5;
            hhi += share * share;
        }
        out.bid = {hhi, ComputeStatus::OK};
    }

    if (ask5 == 0.0) {
        // BLOCKED: see current unresolved governance decision §DENOM-ZERO
        out.ask = {0.0, ComputeStatus::INVALID};
    } else {
        double hhi = 0.0;
        for (unsigned i = 0; i < kLevels; ++i) {
            const double share = static_cast<double>(v.ask_qtys[i]) / ask5;
            hhi += share * share;
        }
        out.ask = {hhi, ComputeStatus::OK};
    }

    return out;
}


// ── Near-touch depth share (§4.10) ───────────────────────────────────────
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
//
// Canonical spec §4.10 verbatim formula:
//
//   near_touch_depth_share_bid(K) = sum_i=0^{K-1} bid_qtys[i] / DEPTH_BID_5
//   near_touch_depth_share_ask(K) = sum_i=0^{K-1} ask_qtys[i] / DEPTH_ASK_5
//
// K MUST be provided by the caller. There is NO default value for K.
//
// The research-only quantitative reference — NOT EGBOS-SPECIFIED instruction is explicit:
//   "do not pick a value for K. Implement it as a function parameterized by
//    K with no default, or omit it entirely until K is fixed by EGBOS."
//
// Preconditions (caller's responsibility):
//   - K must satisfy 1 <= K <= kLevels. Behaviour for K outside this range
//     is not specified here; callers should assert.
//
// Denominator-zero policy: UNRESOLVED — see current unresolved governance decision §DENOM-ZERO.
// BLOCKED: K is unresolved — see current unresolved governance decision §NEAR-TOUCH-K.
// BLOCKED: see current unresolved governance decision §FEAT-CONTRACT, §DENOM-ZERO.
struct NearTouchResult {
    Result<double> bid;
    Result<double> ask;
};

[[nodiscard]] inline NearTouchResult
near_touch_depth_share_candidate(const QtyView& v, unsigned K) noexcept
{
    // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
    // (see research-only quantitative reference — NOT EGBOS-SPECIFIED)
    // K must be provided by caller; no default exists.
    // BLOCKED: see current unresolved governance decision §NEAR-TOUCH-K, §FEAT-CONTRACT, §DENOM-ZERO.

    // Guard: K must be in [1, kLevels]. If caller passes 0 or > kLevels,
    // return INVALID rather than accessing out-of-bounds array.
    if (K == 0 || K > kLevels) {
        return {{0.0, ComputeStatus::INVALID}, {0.0, ComputeStatus::INVALID}};
    }

    const double bid5 = static_cast<double>(depth_bid_5_candidate(v));
    const double ask5 = static_cast<double>(depth_ask_5_candidate(v));

    NearTouchResult out{};

    double near_bid = 0.0;
    double near_ask = 0.0;
    for (unsigned i = 0; i < K; ++i) {
        near_bid += static_cast<double>(v.bid_qtys[i]);
        near_ask += static_cast<double>(v.ask_qtys[i]);
    }

    if (bid5 == 0.0) {
        // BLOCKED: see current unresolved governance decision §DENOM-ZERO
        out.bid = {0.0, ComputeStatus::INVALID};
    } else {
        out.bid = {near_bid / bid5, ComputeStatus::OK};
    }

    if (ask5 == 0.0) {
        // BLOCKED: see current unresolved governance decision §DENOM-ZERO
        out.ask = {0.0, ComputeStatus::INVALID};
    } else {
        out.ask = {near_ask / ask5, ComputeStatus::OK};
    }

    return out;
}

} // namespace egbos::l2::candidates
