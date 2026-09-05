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

namespace egbos::l1 {

constexpr std::size_t kMaxPriceLevels = 65536; 
constexpr std::size_t kMaxOrders = 131072;

// Order Node for order-by-order tracking. No malloc, uses FixedPool.
struct OrderNode {
    uint64_t order_id;
    uint32_t qty;
    int32_t price_index;
    OrderNode* next;
    OrderNode* prev;
};

// Top 5 Book cache-aligned struct (exactly 64 bytes)
struct alignas(64) Top5BidsAsks {
    int32_t bid_price_offsets[5]; // Price offsets (ticks) from anchor
    int32_t ask_price_offsets[5]; 
    uint16_t bid_qtys[5];         // Quantities
    uint16_t ask_qtys[5];
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

    // Snapshot generation for L1 -> L2 IPC
    Top5BidsAsks generate_top5_snapshot() const noexcept;

private:
    int32_t price_to_index(int64_t price) const noexcept;
    int64_t index_to_price(int32_t index) const noexcept;
    
    // Internal node management
    void link_node(int32_t price_index, OrderNode* node) noexcept;
    void unlink_node(OrderNode* node) noexcept;

    int64_t anchor_price_;
    int64_t tick_size_;

    // Price level representation (O(1) lookups)
    struct PriceLevel {
        uint32_t total_qty{0};
        OrderNode* head{nullptr};
        OrderNode* tail{nullptr};
    };

    // Contiguous array of price levels
    std::array<PriceLevel, kMaxPriceLevels> price_levels_;

    // Order lookup using flat array.
    // In an institutional system this is a dense open-addressed hash map or
    // perfectly hashed flat array. Here we use modulo array for demonstration.
    std::array<OrderNode*, kMaxOrders> order_lookup_;

    // Pool allocator for nodes (S2-REQ-005, GC-E-010 compliance)
    egbos::alloc::FixedPool<OrderNode, kMaxOrders> order_pool_;

    int32_t best_bid_index_{-1};
    int32_t best_ask_index_{kMaxPriceLevels};
    
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
