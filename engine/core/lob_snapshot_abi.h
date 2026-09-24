// engine/core/lob_snapshot_abi.h
// EGBOS — L1 -> L2 LOB snapshot transport ABI
//
// This header defines the stable 64-byte payload shared across the
// L1 producer and L2 consumer boundary.
// Quantitative feature mathematics is intentionally out of scope.

#pragma once

#include <cstddef>
#include <cstdint>

namespace egbos::l1 {

// Top 5 Book cache-aligned transport payload.
// Exact ABI: 64 bytes.
struct alignas(64) Top5BidsAsks {
    uint32_t bid_qtys[5];
    uint32_t ask_qtys[5];
    int16_t  bid_price_offsets[5];
    int16_t  ask_price_offsets[5];
    uint32_t anchor_price_low32;
};

static_assert(sizeof(Top5BidsAsks) == 64,
              "Top5BidsAsks must be exactly 64 bytes");
static_assert(alignof(Top5BidsAsks) == 64,
              "Top5BidsAsks must remain 64-byte aligned");

static_assert(offsetof(Top5BidsAsks, bid_qtys) == 0);
static_assert(offsetof(Top5BidsAsks, ask_qtys) == 20);
static_assert(offsetof(Top5BidsAsks, bid_price_offsets) == 40);
static_assert(offsetof(Top5BidsAsks, ask_price_offsets) == 50);
static_assert(offsetof(Top5BidsAsks, anchor_price_low32) == 60);

} // namespace egbos::l1
