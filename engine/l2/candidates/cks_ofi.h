// engine/l2/candidates/cks_ofi.h
// EGBOS — CKS OFI pure formula implementation
//
// PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
// windowing, and price reconstruction are all separately unresolved;
// see research reference; event-stream aggregation and transport remain unresolved.
//
// This file implements the Cont–Kukanov–Stoikov (CKS) event-contribution
// formula e_n exactly as specified in the academic literature and reproduced
// in externally established CKS formula; NOT EGBOS-SPECIFIED for production use. It is provided as groundwork so that when EGBOS
// approves an OFI feature contract, the verified math is already tested.
//
// PROHIBITED: This function must NOT be called from the production
// FeatureEngine::process_instrument() path or any snapshot-processing
// pipeline. It operates on explicit, already-resolved (price, qty) pairs
// and does NOT call any price-reconstruction code.
//
// WHY THIS IS BLOCKED IN PRODUCTION (research reference; production OFI remains blocked by current transport limitations.):
//   1. No ordered, lossless best-quote event stream exists in the current
//      transport — only double-buffered snapshots (a slow/descheduled reader
//      can skip intermediate states).
//   2. Price reconstruction from anchor + offsets is unresolved.
//   3. The verified OFI counterexample (§5.4 below) proves that snapshot
//      pairs with identical endpoints are consistent with materially different
//      OFI values; any snapshot-delta proxy is therefore not OFI.
//   4. SnapshotDeltaOFI or any snapshot-difference proxy must NEVER be named
//      OFI or OFI_1s in code, config, or metrics (No-Invention table, row 6).
//   See current unresolved governance decision §OFI-TRANSPORT, §PRICE-RECON.
//
// ── CKS OFI formula (externally established CKS formula; NOT EGBOS-SPECIFIED for production use verbatim) ─────────────────────
//
// For a sequence of best-quote states (P_n, Q_n) where P = price, Q = qty:
//
//   e_n = contribution of event n to the OFI accumulator
//
//   CKS definition (bid side, single-level):
//     if P_n > P_{n-1}:    e_n = +Q_n         (new best bid at higher price)
//     if P_n = P_{n-1}:    e_n = Q_n - Q_{n-1} (same price, qty change)
//     if P_n < P_{n-1}:    e_n = -Q_{n-1}      (best bid dropped away)
//
//   Ask side is symmetric with sign flipped:
//     if P_n < P_{n-1}:    e_n = -Q_n
//     if P_n = P_{n-1}:    e_n = Q_{n-1} - Q_n
//     if P_n > P_{n-1}:    e_n = +Q_{n-1}
//
//   OFI over a window = sum of bid e_n contributions - sum of ask e_n contributions.
//
// ── Verified counterexample (verified research counterexample; NOT EGBOS-SPECIFIED for production use) ───────────────────────
//
// State: (bid_price=100, bid_qty=10), (ask_price=101, ask_qty=10).
//
// Path A (no events between state 0 and state 1):
//   State 0 = State 1 = (100, 10). OFI_A = 0.
//
// Path B (three events between state 0 and final state):
//   Event 1: (100,10) → (100,50)   e_n = 50-10 = +40  (same price, qty increased)
//   Event 2: (100,50) → (101,5)    e_n = +5            (higher price, new qty)
//   Wait — Path B arrives back at (100,10):
//   Event 3: (101,5)  → (100,10)   e_n = -5            (price dropped, old qty gone)
//   OFI_B = 40 + 5 + (-5) = 40.
//
// Both paths share identical (price=100, qty=10) endpoints.
// OFI_A = 0, OFI_B = 40. This proves snapshot-pair OFI is indeterminate.
//
// The unit tests in test_l2_candidates.cpp verify this exact counterexample.

#pragma once

#include <cstdint>

namespace egbos::l2::candidates::cks {

// ── BestQuoteState ────────────────────────────────────────────────────────
//
// A single already-resolved best-quote state: (price, qty).
// Price is provided as an already-decoded integer (e.g., in exchange native
// units or fixed-point units). This function does NOT accept raw ABI fields
// (anchor_price_low32, bid_price_offsets) — price reconstruction is the
// caller's responsibility, and must only be done once price reconstruction
// semantics are resolved by EGBOS (see current unresolved governance decision §PRICE-RECON).
struct BestQuoteState {
    int64_t  price;  // already-resolved integer price (units: caller-defined)
    uint64_t qty;    // quantity at that price level
};

// ── bid_contribution (externally established CKS formula; NOT EGBOS-SPECIFIED for production use) ────────────────────────────────
//
// PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
// windowing, and price reconstruction are all separately unresolved;
// see research reference; event-stream aggregation and transport remain unresolved.
//
// Computes the CKS bid-side event contribution e_n for a single transition
// from state `prev` to state `curr`.
//
// Formula (externally established CKS formula; NOT EGBOS-SPECIFIED for production use verbatim):
//   if curr.price > prev.price:   return +curr.qty
//   if curr.price == prev.price:  return int64_t(curr.qty) - int64_t(prev.qty)
//   if curr.price < prev.price:   return -int64_t(prev.qty)
//
// Inputs: two consecutive best-bid states (already-resolved prices + qtys).
// Output: signed integer contribution in qty units.
//
// This function is pure, stateless, and has no side effects.
// It must NOT be called from any snapshot-processing pipeline.
[[nodiscard]] inline int64_t
bid_contribution(const BestQuoteState& prev, const BestQuoteState& curr) noexcept
{
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    if (curr.price > prev.price) {
        return static_cast<int64_t>(curr.qty);
    } else if (curr.price == prev.price) {
        return static_cast<int64_t>(curr.qty) - static_cast<int64_t>(prev.qty);
    } else {
        // curr.price < prev.price
        return -static_cast<int64_t>(prev.qty);
    }
}

// ── ask_contribution (externally established CKS formula; NOT EGBOS-SPECIFIED for production use) ────────────────────────────────
//
// PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
// windowing, and price reconstruction are all separately unresolved;
// see research reference; event-stream aggregation and transport remain unresolved.
//
// Computes the CKS ask-side event contribution e_n for a single transition
// from state `prev` to state `curr`. The ask side is symmetric to the bid
// side with sign flipped (externally established CKS formula; NOT EGBOS-SPECIFIED for production use).
//
// Formula (externally established CKS formula; NOT EGBOS-SPECIFIED for production use verbatim):
//   if curr.price < prev.price:   return -curr.qty   (new best ask at lower price)
//   if curr.price == prev.price:  return int64_t(prev.qty) - int64_t(curr.qty)
//   if curr.price > prev.price:   return +int64_t(prev.qty) (ask rose away)
[[nodiscard]] inline int64_t
ask_contribution(const BestQuoteState& prev, const BestQuoteState& curr) noexcept
{
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    if (curr.price < prev.price) {
        return -static_cast<int64_t>(curr.qty);
    } else if (curr.price == prev.price) {
        return static_cast<int64_t>(prev.qty) - static_cast<int64_t>(curr.qty);
    } else {
        // curr.price > prev.price
        return static_cast<int64_t>(prev.qty);
    }
}

// ── ofi_contribution (externally established CKS formula; NOT EGBOS-SPECIFIED for production use) ────────────────────────────────
//
// PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
// windowing, and price reconstruction are all separately unresolved;
// see research reference; event-stream aggregation and transport remain unresolved.
//
// Net OFI contribution of a single event (bid contribution minus ask
// contribution). For a pure event that moves only the bid OR only the ask,
// the other side's "prev == curr" so its contribution is zero.
//
// This is a convenience wrapper for test fixtures. Production OFI
// accumulation over a window is NOT implemented here and must NOT be
// implemented until EGBOS resolves §OFI-TRANSPORT.
[[nodiscard]] inline int64_t
ofi_contribution(
    const BestQuoteState& bid_prev, const BestQuoteState& bid_curr,
    const BestQuoteState& ask_prev, const BestQuoteState& ask_curr) noexcept
{
    // PURE FORMULA IMPLEMENTATION ONLY — event-stream input, aggregation
    // windowing, and price reconstruction are all separately unresolved;
    // see research reference; event-stream aggregation and transport remain unresolved.
    return bid_contribution(bid_prev, bid_curr)
         - ask_contribution(ask_prev, ask_curr);
}

} // namespace egbos::l2::candidates::cks
