// engine/l1/lob_engine.cpp
// EGBOS ENGINE_V3 — L1 Limit Order Book Engine

#include "lob_engine.h"
#include <algorithm>
#include <cstring>

namespace egbos::l1 {

LobEngine::LobEngine(int64_t anchor_price, int64_t tick_size) noexcept
    : anchor_price_(anchor_price), tick_size_(tick_size) {
    order_lookup_.fill(nullptr);
}

int32_t LobEngine::price_to_index(int64_t price) const noexcept {
    // S2-REQ-006: O(1) index calculation using fixed point math
    int64_t diff = price - anchor_price_;
    int64_t ticks = diff / tick_size_;
    int32_t index = static_cast<int32_t>(kMaxPriceLevels / 2 + ticks);
    // Boundary clamp for safety
    if (index < 0) index = 0;
    if (index >= static_cast<int32_t>(kMaxPriceLevels)) index = kMaxPriceLevels - 1;
    return index;
}

int64_t LobEngine::index_to_price(int32_t index) const noexcept {
    int64_t ticks = static_cast<int64_t>(index) - static_cast<int64_t>(kMaxPriceLevels / 2);
    return anchor_price_ + (ticks * tick_size_);
}

void LobEngine::link_node(int32_t price_index, OrderNode* node) noexcept {
    PriceLevel& level = price_levels_[price_index];
    node->prev = level.tail;
    node->next = nullptr;
    
    if (level.tail) {
        level.tail->next = node;
    } else {
        level.head = node;
    }
    level.tail = node;
    level.total_qty += node->qty;
}

void LobEngine::unlink_node(OrderNode* node) noexcept {
    PriceLevel& level = price_levels_[node->price_index];
    level.total_qty -= node->qty;
    
    if (node->prev) node->prev->next = node->next;
    else level.head = node->next;
    
    if (node->next) node->next->prev = node->prev;
    else level.tail = node->prev;
}

void LobEngine::add_order(uint64_t order_id, int64_t price, uint32_t qty, bool is_bid) noexcept {
    uint32_t slot = order_id % kMaxOrders;
    if (order_lookup_[slot] != nullptr) [[unlikely]] return; // Collision stub

    OrderNode* node = order_pool_.acquire();
    if (!node) [[unlikely]] return; // Pool exhausted (Fatal in prod)

    int32_t p_idx = price_to_index(price);
    node->order_id = order_id;
    node->qty = qty;
    node->price_index = p_idx;

    link_node(p_idx, node);
    order_lookup_[slot] = node;

    // Update BBO trackers
    if (is_bid && p_idx > best_bid_index_) {
        best_bid_index_ = p_idx;
    } else if (!is_bid && p_idx < best_ask_index_) {
        best_ask_index_ = p_idx;
    }
}

void LobEngine::modify_order(uint64_t order_id, uint32_t new_qty) noexcept {
    uint32_t slot = order_id % kMaxOrders;
    OrderNode* node = order_lookup_[slot];
    if (!node || node->order_id != order_id) [[unlikely]] return;

    PriceLevel& level = price_levels_[node->price_index];
    level.total_qty -= node->qty;
    level.total_qty += new_qty;
    node->qty = new_qty;
}

void LobEngine::cancel_order(uint64_t order_id) noexcept {
    uint32_t slot = order_id % kMaxOrders;
    OrderNode* node = order_lookup_[slot];
    if (!node || node->order_id != order_id) [[unlikely]] return;

    int32_t p_idx = node->price_index;
    unlink_node(node);
    order_lookup_[slot] = nullptr;
    order_pool_.release(node);

    // If we removed from BBO and level became empty, update BBO
    if (p_idx == best_bid_index_ && price_levels_[p_idx].total_qty == 0) {
        update_bbo();
    } else if (p_idx == best_ask_index_ && price_levels_[p_idx].total_qty == 0) {
        update_bbo();
    }
}

void LobEngine::update_bbo() noexcept {
    // Scan downwards for best bid
    while (best_bid_index_ >= 0 && price_levels_[best_bid_index_].total_qty == 0) {
        best_bid_index_--;
    }
    // Scan upwards for best ask
    while (best_ask_index_ < static_cast<int32_t>(kMaxPriceLevels) && price_levels_[best_ask_index_].total_qty == 0) {
        best_ask_index_++;
    }
}

Top5BidsAsks LobEngine::generate_top5_snapshot() const noexcept {
    Top5BidsAsks snap;
    std::memset(&snap, 0, sizeof(Top5BidsAsks));
    snap.anchor_price_low32 = static_cast<uint32_t>(anchor_price_ & 0xFFFFFFFF);

    // Top 5 bids
    int count = 0;
    for (int32_t i = best_bid_index_; i >= 0 && count < 5; --i) {
        if (price_levels_[i].total_qty > 0) {
            snap.bid_price_offsets[count] = i - static_cast<int32_t>(kMaxPriceLevels / 2);
            snap.bid_qtys[count] = static_cast<uint16_t>(std::min(price_levels_[i].total_qty, 65535u));
            count++;
        }
    }

    // Top 5 asks
    count = 0;
    for (int32_t i = best_ask_index_; i < static_cast<int32_t>(kMaxPriceLevels) && count < 5; ++i) {
        if (price_levels_[i].total_qty > 0) {
            snap.ask_price_offsets[count] = i - static_cast<int32_t>(kMaxPriceLevels / 2);
            snap.ask_qtys[count] = static_cast<uint16_t>(std::min(price_levels_[i].total_qty, 65535u));
            count++;
        }
    }

    return snap;
}

} // namespace egbos::l1
