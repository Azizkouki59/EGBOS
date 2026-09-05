// engine/l1/lob_engine.cpp
// EGBOS ENGINE_V3 — L1 Limit Order Book Engine

#include "lob_engine.h"
#include <algorithm>
#include <cstring>
#include "../core/snapshot_abi.h"

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
    uint32_t initial_slot = slot;
    while (order_lookup_[slot] != nullptr) {
        if (order_lookup_[slot]->order_id == order_id) [[unlikely]] return; // Duplicate order_id
        slot = (slot + 1) % kMaxOrders;
        if (slot == initial_slot) [[unlikely]] return; // Table full
    }

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
    uint32_t initial_slot = slot;
    while (order_lookup_[slot] != nullptr) {
        if (order_lookup_[slot]->order_id == order_id) {
            break;
        }
        slot = (slot + 1) % kMaxOrders;
        if (slot == initial_slot) [[unlikely]] return;
    }
    OrderNode* node = order_lookup_[slot];
    if (!node) [[unlikely]] return;

    PriceLevel& level = price_levels_[node->price_index];
    level.total_qty -= node->qty;
    level.total_qty += new_qty;
    node->qty = new_qty;
}

void LobEngine::cancel_order(uint64_t order_id) noexcept {
    uint32_t slot = order_id % kMaxOrders;
    uint32_t initial_slot = slot;
    while (order_lookup_[slot] != nullptr) {
        if (order_lookup_[slot]->order_id == order_id) {
            break;
        }
        slot = (slot + 1) % kMaxOrders;
        if (slot == initial_slot) [[unlikely]] return;
    }
    OrderNode* node = order_lookup_[slot];
    if (!node) [[unlikely]] return;

    int32_t p_idx = node->price_index;
    unlink_node(node);

    // Shift elements back to preserve linear probing chains
    order_lookup_[slot] = nullptr;
    uint32_t i = (slot + 1) % kMaxOrders;
    while (order_lookup_[i] != nullptr) {
        OrderNode* next_node = order_lookup_[i];
        uint32_t ideal_slot = next_node->order_id % kMaxOrders;
        uint32_t dist_to_i = (i + kMaxOrders - ideal_slot) % kMaxOrders;
        uint32_t dist_to_slot = (slot + kMaxOrders - ideal_slot) % kMaxOrders;

        if (dist_to_slot < dist_to_i) {
            order_lookup_[slot] = next_node;
            order_lookup_[i] = nullptr;
            slot = i;
        }
        i = (i + 1) % kMaxOrders;
    }

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

size_t LobEngine::snapshot_into(uint8_t* buf, size_t buf_len) const noexcept {
    if (buf_len < sizeof(LobInternalSnapshot)) return 0;

    // Count active orders
    uint32_t active_orders = 0;
    for (size_t i = 0; i < kMaxPriceLevels; ++i) {
        auto* curr = price_levels_[i].head;
        while (curr) { active_orders++; curr = curr->next; }
    }

    const size_t needed = sizeof(LobInternalSnapshot) + active_orders * sizeof(egbos::core::SerializedOrder);
    if (buf_len < needed) return 0;

    LobInternalSnapshot hdr;
    hdr.anchor_price    = anchor_price_;
    hdr.tick_size       = tick_size_;
    hdr.best_bid_index  = best_bid_index_;
    hdr.best_ask_index  = best_ask_index_;
    hdr.order_count     = active_orders;

    size_t offset = 0;
    std::memcpy(buf + offset, &hdr, sizeof(hdr));
    offset += sizeof(hdr);

    for (size_t i = 0; i < kMaxPriceLevels; ++i) {
        const size_t level_start = offset;
        size_t num_in_level = 0;
        auto* curr = price_levels_[i].head;
        while (curr) {
            egbos::core::SerializedOrder so{};
            so.order_id    = curr->order_id;
            so.qty         = curr->qty;
            so.price_index = curr->price_index;
            so._pad        = 0;
            std::memcpy(buf + offset, &so, sizeof(so));
            offset += sizeof(so);
            num_in_level++;
            curr = curr->next;
        }
        // Sort by order_id within the level
        if (num_in_level > 1) {
            auto* start = reinterpret_cast<egbos::core::SerializedOrder*>(buf + level_start);
            std::sort(start, start + num_in_level,
                [](const egbos::core::SerializedOrder& a, const egbos::core::SerializedOrder& b) {
                    return a.order_id < b.order_id;
                });
        }
    }

    return offset;
}

bool LobEngine::hydrate_from(const uint8_t* buf, size_t len) noexcept {
    if (len < sizeof(LobInternalSnapshot)) return false;

    LobInternalSnapshot hdr;
    std::memcpy(&hdr, buf, sizeof(hdr));
    size_t offset = sizeof(hdr);

    if (hdr.order_count > kMaxOrders) return false;
    if (hdr.best_bid_index < -1 || hdr.best_bid_index >= static_cast<int32_t>(kMaxPriceLevels)) return false;
    if (hdr.best_ask_index < 0 || hdr.best_ask_index > static_cast<int32_t>(kMaxPriceLevels)) return false;
    if (hdr.tick_size <= 0) return false;

    if (len < offset + static_cast<size_t>(hdr.order_count) * sizeof(egbos::core::SerializedOrder)) return false;

    // Pre-validate all orders before altering existing book state
    for (uint32_t i = 0; i < hdr.order_count; ++i) {
        egbos::core::SerializedOrder so;
        std::memcpy(&so, buf + offset + i * sizeof(so), sizeof(so));
        if (so.price_index < 0 || so.price_index >= static_cast<int32_t>(kMaxPriceLevels)) {
            return false;
        }
    }

    // Clear existing book state
    for (auto& level : price_levels_) {
        level.head      = nullptr;
        level.tail      = nullptr;
        level.total_qty = 0;
    }
    for (auto& ptr : order_lookup_) {
        if (ptr) { order_pool_.release(ptr); ptr = nullptr; }
    }

    anchor_price_    = hdr.anchor_price;
    tick_size_       = hdr.tick_size;
    best_bid_index_  = hdr.best_bid_index;
    best_ask_index_  = hdr.best_ask_index;

    for (uint32_t i = 0; i < hdr.order_count; ++i) {
        egbos::core::SerializedOrder so;
        std::memcpy(&so, buf + offset, sizeof(so));
        offset += sizeof(so);

        OrderNode* node = order_pool_.acquire();
        if (!node) return false;
        node->order_id    = so.order_id;
        node->qty         = so.qty;
        node->price_index = so.price_index;
        node->next        = nullptr;
        node->prev        = nullptr;

        link_node(so.price_index, node);

        uint32_t slot         = so.order_id % kMaxOrders;
        uint32_t initial_slot = slot;
        while (order_lookup_[slot] != nullptr) {
            if (order_lookup_[slot]->order_id == so.order_id) {
                order_pool_.release(node);
                return false;
            }
            slot = (slot + 1) % kMaxOrders;
            if (slot == initial_slot) {
                order_pool_.release(node);
                return false;
            }
        }
        order_lookup_[slot] = node;
    }
    return true;
}

} // namespace egbos::l1
