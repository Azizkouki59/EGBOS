// engine/l1/lob_engine.h
// EGBOS ENGINE_V3 — L1 Limit Order Book Engine
//
// Claims         : S2-REQ-005, I-S2-004
// Path           : engine/l1/lob_engine.h

#pragma once

#include <cstdint>
#include <array>
#include "engine/core/fixed_point.h"
#include "engine/core/pool_allocator.h"
#include "engine/core/shm_layout.h"
#include "engine/l1/depth_parser.h"  // ParsedDepthUpdate

namespace egbos::l1 {

constexpr std::size_t kMaxPriceLevels = 65536; 
constexpr std::size_t kMaxOrders = 131072;

// Order Node for order-by-order tracking. No malloc, uses FixedPool.
struct OrderNode {
    uint64_t order_id;
    uint32_t qty;
    int32_t price_index;
    bool is_bid;
    OrderNode* next;
    OrderNode* prev;
};

// Top 5 Book cache-aligned struct (exactly 64 bytes)
struct alignas(64) Top5BidsAsks {
    uint32_t bid_qtys[5];         // Quantities (widened to 32-bit to avoid 10^7 saturation)
    uint32_t ask_qtys[5];
    int16_t  bid_price_offsets[5]; // Price offsets from anchor (fits in int16_t since kMaxPriceLevels=65536)
    int16_t  ask_price_offsets[5];
    uint32_t anchor_price_low32;  // Lowest 32 bits of anchor or metadata
};
static_assert(sizeof(Top5BidsAsks) == 64, "Top5BidsAsks must be exactly 64 bytes for L2 cache alignment");

// Flat array limit order book (Zero pointer chasing across heap)
class LobEngine {
public:
    explicit LobEngine(int64_t anchor_price, int64_t tick_size) noexcept;

    // Zero-allocation order operations
    void add_order(uint64_t order_id, int64_t price, uint32_t qty, bool is_bid) noexcept;
    void modify_order(uint64_t order_id, uint32_t new_qty) noexcept;
    void cancel_order(uint64_t order_id) noexcept;

    // ── Production depth-update application API ───────────────────────────────
    // apply_depth_update: consumes ONE complete ParsedDepthUpdate (one Binance
    // @depth frame) and applies all bid and ask levels atomically at the frame
    // boundary.  This is the correct production API for the CLI and any consumer
    // that receives a complete frame at a time rather than per-level events.
    //
    // The caller is responsible for Binance U/u sequence validation BEFORE calling
    // this function.  apply_depth_update does not perform gap detection — it simply
    // applies each level in the frame and updates last_update_id_ to
    // update.final_update_id.
    //
    // Quantity == 0 → remove the price level (Binance deletion semantics).
    // Quantity  > 0 → insert or replace the price level.
    void apply_depth_update(const ParsedDepthUpdate& update) noexcept;

    // ── Synthetic level mutation (test / L1Consumer path) ─────────────────────
    // Called by L1Consumer for each price-level event (per-level slot path).
    // Internally selects add_order / modify_order / cancel_order based on live
    // order_lookup_ state — L1Consumer must NOT duplicate this decision.
    // See SYNTHETIC ID COLLISION-SAFETY CONSTRAINT comment in lob_engine.cpp.
    void upsert_synthetic_level(int64_t price, uint32_t qty, bool is_bid) noexcept;

    // Snapshot generation for L1 -> L2 IPC
    Top5BidsAsks generate_top5_snapshot() const noexcept;

    // ── Read-only BBO accessors (CLI / diagnostic use) ────────────────────────
    struct Level {
        int64_t price;
        int64_t qty;
    };
    // Fills vectors with up to `limit` active bid and ask levels.
    void get_book_levels(std::vector<Level>& out_bids, std::vector<Level>& out_asks, int limit) const noexcept;

    // Returns fixed-point price/quantity at the best bid/ask, or 0 if empty.
    // Price scale: kPriceScale (10^7) — same as depth_parser.h.
    int64_t  best_bid_price() const noexcept;
    int64_t  best_ask_price() const noexcept;
    int64_t  best_bid_qty()   const noexcept;
    int64_t  best_ask_qty()   const noexcept;

    // Last applied Binance final_update_id (u), updated by apply_depth_update.
    uint64_t last_update_id() const noexcept { return last_update_id_; }

    // Total number of complete depth frames successfully applied.
    uint64_t depth_frames_applied() const noexcept { return depth_frames_applied_; }
    uint64_t levels_applied()       const noexcept { return levels_applied_; }

private:
    int32_t price_to_index(int64_t price) const noexcept;
    int64_t index_to_price(int32_t index) const noexcept;
    
    // Internal node management
    void link_node(int32_t price_index, OrderNode* node, bool is_bid) noexcept;
    void unlink_node(OrderNode* node) noexcept;

    int64_t anchor_price_;
    int64_t tick_size_;

    // Price level representation (O(1) lookups)
    struct PriceLevel {
        uint32_t total_qty{0};
        OrderNode* head{nullptr};
        OrderNode* tail{nullptr};
    };

    // Contiguous array of price levels (physically split to prevent cross-book corruption)
    std::array<PriceLevel, kMaxPriceLevels> bid_levels_;
    std::array<PriceLevel, kMaxPriceLevels> ask_levels_;

    // Order lookup using flat array.
    // In an institutional system this is a dense open-addressed hash map or
    // perfectly hashed flat array. Here we use modulo array for demonstration.
    std::array<OrderNode*, kMaxOrders> order_lookup_;

    // Pool allocator for nodes (S2-REQ-005, GC-E-010 compliance)
    egbos::alloc::FixedPool<OrderNode, kMaxOrders> order_pool_;

    int32_t  best_bid_index_{-1};
    int32_t  best_ask_index_{kMaxPriceLevels};
    uint64_t last_update_id_{0};         // tracks Binance u for apply_depth_update path
    uint64_t depth_frames_applied_{0};   // count of complete frames applied
    uint64_t levels_applied_{0};         // count of individual level mutations applied

    void update_bbo() noexcept;

public:
    struct LobInternalSnapshot {
        int64_t  anchor_price;
        int64_t  tick_size;
        int32_t  best_bid_index;
        int32_t  best_ask_index;
        uint32_t order_count;
    };

    size_t snapshot_into(uint8_t* buf, size_t buf_len) const noexcept;
    bool hydrate_from(const uint8_t* buf, size_t len) noexcept;
};

} // namespace egbos::l1
