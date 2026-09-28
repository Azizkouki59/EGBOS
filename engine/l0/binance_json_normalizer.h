// engine/l0/binance_json_normalizer.h
// EGBOS ENGINE_V3 — L0 Binance JSON Normalizer
//
// Specification  : S2-REQ-001 (Normalization Pipeline)
// Constraint     : Zero dynamic allocation (GC-E-010).
//                  No dependency on L1 types — L0 must not reverse-depend on L1.
//
// Parses one Binance @depth WebSocket JSON frame and emits one RawMarketEvent
// per price level (bid or ask).  The caller supplies a callback that is invoked
// for every parsed level so it can push events to the SPSC ring.
//
// PRICE_SCALE
// -----------
// kL0PriceScale = 100'000 (10^5) — matches wal_writer.h kPriceScale and spec
// mandate.  depth_parser.h used 10^7 (F4); this file uses the correct value.
//
// Sequence number
// ---------------
// The Binance @depth JSON field "u" (final_update_id) is extracted and stored
// in RawMarketEvent::sequence_num_.  Field "U" (first_update_id) is skipped for
// now; gap detection is deferred to a future ADR (S2-REQ-002).

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>

#include "market_event_generated.h"

namespace egbos::l0 {

// ── PRICE_SCALE ──────────────────────────────────────────────────────────────
// 10^5 per spec (wal_writer.h §4.4, S2-REQ-001).
// All fixed-point prices in RawMarketEvent use this scale.
inline constexpr int64_t kL0PriceScale = 100'000LL;

// ── ParseStatus ──────────────────────────────────────────────────────────────
enum class L0ParseStatus : uint8_t {
    OK                      = 0,
    TRUNCATED               = 1,  // Input ended mid-parse
    MISSING_FIELD           = 2,  // Required JSON key absent
    WRONG_EVENT_TYPE        = 3,  // "e" != "depthUpdate"
    NUMERIC_PARSE_FAILURE   = 4,
    SCIENTIFIC_NOTATION     = 5,  // 'e'/'E' in price string — rejected
    PRECISION_EXCEEDED      = 6,  // More decimal digits than scale supports
    OVERFLOW                = 7,
    WRONG_ARITY             = 8,  // Inner [price,qty] array != 2 elements
    ARRAY_TOO_LARGE         = 9,
};

// Maximum levels per @depth packet.  Binance typically sends ≤20 levels per
// update (diff feed), but the spec supports up to 256.
inline constexpr std::size_t kMaxL0Levels = 256;

// ── Callback type ─────────────────────────────────────────────────────────────
// Called once for every successfully parsed price level.
// `event` is a fully constructed, value-initialized RawMarketEvent ready to
// be placed into a MarketEventSlot.
// `first_update_id` is the Binance U field of the frame this level belongs to.
// Return false from the callback to abort processing further levels.
using L0EventCallback = bool (*)(
    const egbos::events::RawMarketEvent& event,
    uint64_t first_update_id,
    uint32_t frame_level_index,
    void* user_ctx) noexcept;

// ── normalize_binance_depth ───────────────────────────────────────────────────
// Parse `json[0..length)` as a Binance @depth diff frame.
// Invokes `cb` once per price level, passing a constructed RawMarketEvent.
//
// Parameters:
//   json            — raw UTF-8 bytes (NOT NUL-terminated required)
//   length          — byte count of the JSON frame
//   ingestion_tsc   — CLOCK_MONOTONIC_RAW value recorded at receipt (L0 field)
//   cb              — caller-supplied callback, one call per level
//   user_ctx        — opaque pointer passed through to cb
//
// Returns L0ParseStatus::OK if all mandatory fields were present and at least
// one side had entries.  Returns an error code on the first parse failure;
// cb may have been called zero or more times before the error.
L0ParseStatus normalize_binance_depth(
    const uint8_t*  json,
    std::size_t     length,
    uint64_t        ingestion_tsc,
    L0EventCallback cb,
    void*           user_ctx) noexcept;

// ── Trade callback ────────────────────────────────────────────────────────────
using L0TradeEventCallback = bool (*)(
    const egbos::events::RawMarketEvent& event,
    void* user_ctx) noexcept;

// ── normalize_binance_trade ──────────────────────────────────────────────────
// Parse one Binance <symbol>@trade JSON frame without dynamic allocation.
// Required fields: e, s, t, p, q, T, m.
//
// Mapping:
//   t -> sequence_num
//   p -> price (10^5 fixed point)
//   q -> quantity (uint32_t, overflow rejected)
//   T -> exchange_tsc (milliseconds converted to nanoseconds)
//   ingestion_tsc -> ingestion_tsc
//   m=true  -> Side_SELL
//   m=false -> Side_BUY
L0ParseStatus normalize_binance_trade(
    const uint8_t*       json,
    std::size_t          length,
    uint64_t             ingestion_tsc,
    L0TradeEventCallback cb,
    void*                user_ctx) noexcept;

} // namespace egbos::l0
