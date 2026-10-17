// engine/core/lob_snapshot_abi.h
// EGBOS — L1 -> L2 LOB snapshot transport ABI
//
// This header defines the stable 128-byte payload shared across the
// L1 producer and L2 consumer boundary.
//
// §PRICE-RECON resolution (ABI v2):
//   The original 64-byte struct carried only anchor_price_low32 (uint32_t),
//   which prevented L2 from reconstructing absolute prices.  This version
//   replaces anchor_price_low32 with the full int64_t anchor_price and adds
//   int64_t tick_size, completing the information needed for L2 to call
//   calculate_absolute_price().
//
// ABI VERSION HISTORY
//   v1 (64  bytes): anchor_price_low32 only — §PRICE-RECON blocker
//   v2 (128 bytes): anchor_price (int64_t) + tick_size (int64_t) — this file
//
// Layout (128 bytes, alignas(64)):
//   [ 0 – 19]  bid_qtys[5]           uint32_t[5]  20 bytes
//   [20 – 39]  ask_qtys[5]           uint32_t[5]  20 bytes
//   [40 – 49]  bid_price_offsets[5]  int16_t[5]   10 bytes
//   [50 – 59]  ask_price_offsets[5]  int16_t[5]   10 bytes
//   [60 – 63]  _reserved0            uint32_t      4 bytes (alignment padding)
//   [64 – 71]  anchor_price          int64_t       8 bytes
//   [72 – 79]  tick_size             int64_t       8 bytes
//   [80 – 87]  source_event_ts_ns    uint64_t      8 bytes (L0 ingestion timestamp, ns)
//   [88 –127]  _reserved[40]         uint8_t[40]  40 bytes (forward compat)
//
// §TIMESTAMP-DETERMINISM (ABI v2.1, zero-size change):
//   source_event_ts_ns carries the CLOCK_MONOTONIC_RAW nanosecond timestamp
//   captured at L0 socket receive (RawMarketEvent::ingestion_tsc).  L2 uses
//   this field for Hawkes decay (Δt) instead of calling clock_gettime,
//   achieving full replay determinism under P-003 / A1.6.
//
// Price reconstruction:
//   The L1 engine stores prices as: index_to_price(i) = anchor_price + (i - N/2) * tick_size
//   where N = kMaxPriceLevels and i is the price-level index.
//   The stored offset in Top5BidsAsks is: level_offset = index - N/2
//   Therefore: absolute_price = anchor_price + level_offset * tick_size
//   Use calculate_absolute_price(level_offset) for overflow-safe reconstruction.

#pragma once

#include <cstddef>
#include <cstdint>

namespace egbos::l1 {

// Top 5 Book transport payload.
// Exactly 128 bytes, alignas(64) (two cache lines, first-line aligned).
// See §PRICE-RECON resolution notes in file header.
struct alignas(64) Top5BidsAsks {
    // ── Quantities ──────────────────────────────────────────────────────
    uint32_t bid_qtys[5];           // bytes  0–19
    uint32_t ask_qtys[5];           // bytes 20–39

    // ── Price offsets (index - kMaxPriceLevels/2) ────────────────────────
    // To reconstruct: absolute_price = anchor_price + offset * tick_size
    int16_t  bid_price_offsets[5];  // bytes 40–49
    int16_t  ask_price_offsets[5];  // bytes 50–59

    // ── Alignment padding ────────────────────────────────────────────────
    uint32_t _reserved0;            // bytes 60–63: must be zero

    // ── §PRICE-RECON fields (v2, replaces anchor_price_low32) ────────────
    int64_t  anchor_price;          // bytes 64–71: full 64-bit anchor
    int64_t  tick_size;             // bytes 72–79: tick size (positive)

    // ── §TIMESTAMP-DETERMINISM field (v2.1, zero-size change) ─────────────
    // Carries the L0 ingestion timestamp (CLOCK_MONOTONIC_RAW, ns) from
    // RawMarketEvent::ingestion_tsc.  L2 uses this for Hawkes Δt so that
    // offline replay produces bit-identical feature values (P-003 / A1.6).
    uint64_t source_event_ts_ns;    // bytes 80– 87: L0 socket receive timestamp

    // ── Reserved for forward-compatible ABI growth ────────────────────────
    uint8_t  _reserved[40];         // bytes 88–127: must be zero on write

    // ── Price reconstruction helper ───────────────────────────────────────
    //
    // Returns the exact absolute price for a given level_offset.
    // level_offset is (price_index - kMaxPriceLevels/2) as stored in
    // bid_price_offsets[] / ask_price_offsets[].
    //
    // Uses __builtin_mul_overflow / __builtin_add_overflow to saturate
    // rather than invoke undefined behavior on overflow.
    //
    // PRECONDITION: tick_size > 0 (enforced by LobEngine constructor).
    [[nodiscard]] inline int64_t calculate_absolute_price(
        int16_t level_offset) const noexcept
    {
        int64_t scaled;
        if (__builtin_mul_overflow(
                static_cast<int64_t>(level_offset), tick_size, &scaled)) {
            return (level_offset < 0) ? INT64_MIN : INT64_MAX;
        }
        int64_t result;
        if (__builtin_add_overflow(anchor_price, scaled, &result)) {
            return (scaled < 0) ? INT64_MIN : INT64_MAX;
        }
        return result;
    }
};

static_assert(sizeof(Top5BidsAsks) == 128,
              "Top5BidsAsks must be exactly 128 bytes (ABI v2.1)");
static_assert(alignof(Top5BidsAsks) == 64,
              "Top5BidsAsks must remain 64-byte aligned");

static_assert(offsetof(Top5BidsAsks, bid_qtys)          ==   0);
static_assert(offsetof(Top5BidsAsks, ask_qtys)          ==  20);
static_assert(offsetof(Top5BidsAsks, bid_price_offsets) ==  40);
static_assert(offsetof(Top5BidsAsks, ask_price_offsets) ==  50);
static_assert(offsetof(Top5BidsAsks, _reserved0)        ==  60);
static_assert(offsetof(Top5BidsAsks, anchor_price)      ==  64);
static_assert(offsetof(Top5BidsAsks, tick_size)             ==  72);
static_assert(offsetof(Top5BidsAsks, source_event_ts_ns)    ==  80);
static_assert(offsetof(Top5BidsAsks, _reserved)             ==  88);

} // namespace egbos::l1
