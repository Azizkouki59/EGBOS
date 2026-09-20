// engine/l1/lob_engine.cpp
// EGBOS ENGINE_V3 — L1 Limit Order Book Engine

#include "lob_engine.h"
#include <algorithm>
#include <cstring>
#include "../core/snapshot_abi.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace egbos::l1 {

namespace {
[[noreturn]] inline void lob_fatal(const char* msg) noexcept {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "EGBOS TERMINAL_HALT: LobEngine fatal error: %s\n", msg);
    (void)::write(STDERR_FILENO, buf, std::strlen(buf));
    std::abort();
}
} // namespace

LobEngine::LobEngine(int64_t anchor_price, int64_t tick_size) noexcept
    : anchor_price_(anchor_price), tick_size_(tick_size) {
    if (tick_size_ <= 0) {
        lob_fatal("tick_size must be positive");
    }
    order_lookup_.fill(nullptr);
    std::memset(bid_levels_.data(), 0, sizeof(bid_levels_));
    std::memset(ask_levels_.data(), 0, sizeof(ask_levels_));
}

int32_t LobEngine::price_to_index(int64_t price) const noexcept {
    // S2-REQ-006: O(1) index calculation using fixed point math
    // Uses __builtin_sub_overflow to detect overflow without invoking UB;
    // any plain subtraction between arbitrary int64_t values can overflow.
    // See crash-70b360f1831aa3254453f07c90340be34b3aa410.
    int64_t diff;
    if (__builtin_sub_overflow(price, anchor_price_, &diff)) {
        return (price < anchor_price_) ? 0 : static_cast<int32_t>(kMaxPriceLevels) - 1;
    }
    int64_t ticks = diff / tick_size_;
    int32_t index = static_cast<int32_t>(kMaxPriceLevels / 2 + ticks);
    // Boundary clamp for safety
    if (index < 0) index = 0;
    if (index >= static_cast<int32_t>(kMaxPriceLevels)) index = kMaxPriceLevels - 1;
    return index;
}

int64_t LobEngine::index_to_price(int32_t index) const noexcept {
    // Overflow-safe: see crash-70b360f1831aa3254453f07c90340be34b3aa410
    // for the analogous bug in price_to_index. Saturates rather than
    // invoking UB on out-of-range index values.
    int64_t ticks = index - static_cast<int32_t>(kMaxPriceLevels / 2);
    int64_t offset;
    if (__builtin_mul_overflow(ticks, tick_size_, &offset)) {
        return (ticks < 0) ? INT64_MIN : INT64_MAX;
    }
    int64_t price;
    if (__builtin_add_overflow(anchor_price_, offset, &price)) {
        return (offset < 0) ? INT64_MIN : INT64_MAX;
    }
    return price;
}

void LobEngine::link_node(int32_t price_index, OrderNode* node, bool is_bid) noexcept {
    PriceLevel& level = is_bid ? bid_levels_[price_index] : ask_levels_[price_index];
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
    PriceLevel& level = node->is_bid ? bid_levels_[node->price_index] : ask_levels_[node->price_index];
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
    node->is_bid = is_bid;

    link_node(p_idx, node, is_bid);
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

    PriceLevel& level = node->is_bid ? bid_levels_[node->price_index] : ask_levels_[node->price_index];
    level.total_qty -= node->qty;
    level.total_qty += new_qty;
    node->qty = new_qty;

    if (level.total_qty == 0) {
        if (node->is_bid && node->price_index == best_bid_index_) {
            update_bbo();
        } else if (!node->is_bid && node->price_index == best_ask_index_) {
            update_bbo();
        }
    }
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
    bool is_bid = node->is_bid;
    unlink_node(node);

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

    if (is_bid) {
        if (p_idx == best_bid_index_ && bid_levels_[p_idx].total_qty == 0) {
            update_bbo();
        }
    } else {
        if (p_idx == best_ask_index_ && ask_levels_[p_idx].total_qty == 0) {
            update_bbo();
        }
    }
}

void LobEngine::upsert_synthetic_level(int64_t price, uint32_t qty, bool is_bid) noexcept {
    const int32_t price_index = price_to_index(price);

    // Include is_bid in the synthetic_id so bids and asks at the same price never collide.
    const uint64_t synthetic_id = 0x8000000000000000ULL | (is_bid ? 0x4000000000000000ULL : 0) | static_cast<uint64_t>(price_index);

    if (qty == 0) {
        cancel_order(synthetic_id);
        return;
    }

    uint32_t slot = static_cast<uint32_t>(synthetic_id % kMaxOrders);
    uint32_t initial_slot = slot;
    bool found = false;
    while (order_lookup_[slot] != nullptr) {
        if (order_lookup_[slot]->order_id == synthetic_id) {
            found = true;
            break;
        }
        slot = (slot + 1) % kMaxOrders;
        if (slot == initial_slot) [[unlikely]] break;
    }

    if (found) {
        modify_order(synthetic_id, qty);
    } else {
        add_order(synthetic_id, price, qty, is_bid);
    }
}

void LobEngine::update_bbo() noexcept {
    while (best_bid_index_ >= 0 && bid_levels_[best_bid_index_].total_qty == 0) {
        best_bid_index_--;
    }
    while (best_ask_index_ < static_cast<int32_t>(kMaxPriceLevels) && ask_levels_[best_ask_index_].total_qty == 0) {
        best_ask_index_++;
    }
}

// ── apply_depth_update ─────────────────────────────────────────────────────────────
void LobEngine::apply_depth_update(const ParsedDepthUpdate& update) noexcept {
    // Apply all bid levels from the frame.
    // Binance semantics: qty == 0 means remove the price level, qty > 0 means
    // insert-or-replace.  We use the same synthetic-ID scheme as
    // upsert_synthetic_level to represent price levels without real order IDs.
    for (std::size_t i = 0; i < update.bid_count; ++i) {
        const int64_t price = update.bids[i].price;
        const int64_t qty   = update.bids[i].qty;
        
        upsert_synthetic_level(price, static_cast<uint32_t>(qty), /*is_bid=*/true);
        ++levels_applied_;
    }
    // Apply all ask levels from the frame.
    for (std::size_t i = 0; i < update.ask_count; ++i) {
        const int64_t price = update.asks[i].price;
        const int64_t qty   = update.asks[i].qty;
        
        upsert_synthetic_level(price, static_cast<uint32_t>(qty), /*is_bid=*/false);
        ++levels_applied_;
    }
    // Advance the local book's update ID to the frame's final_update_id.
    last_update_id_ = update.final_update_id;
    ++depth_frames_applied_;
}

// ── BBO read-only accessors ─────────────────────────────────────────────────────────
int64_t LobEngine::best_bid_price() const noexcept {
    if (best_bid_index_ < 0) return 0;
    return index_to_price(best_bid_index_);
}
int64_t LobEngine::best_ask_price() const noexcept {
    if (best_ask_index_ >= static_cast<int32_t>(kMaxPriceLevels)) return 0;
    return index_to_price(best_ask_index_);
}
int64_t LobEngine::best_bid_qty() const noexcept {
    if (best_bid_index_ < 0) return 0;
    return static_cast<int64_t>(bid_levels_[best_bid_index_].total_qty);
}
int64_t LobEngine::best_ask_qty() const noexcept {
    if (best_ask_index_ >= static_cast<int32_t>(kMaxPriceLevels)) return 0;
    return static_cast<int64_t>(ask_levels_[best_ask_index_].total_qty);
}

void LobEngine::get_book_levels(std::vector<Level>& out_bids, std::vector<Level>& out_asks, int limit) const noexcept {
    out_bids.clear();
    out_asks.clear();
    int count = 0;
    for (int32_t i = best_bid_index_; i >= 0 && count < limit; --i) {
        if (bid_levels_[i].total_qty > 0) {
            out_bids.push_back({index_to_price(i), static_cast<int64_t>(bid_levels_[i].total_qty)});
            count++;
        }
    }
    count = 0;
    for (int32_t i = best_ask_index_; i < static_cast<int32_t>(kMaxPriceLevels) && count < limit; ++i) {
        if (ask_levels_[i].total_qty > 0) {
            out_asks.push_back({index_to_price(i), static_cast<int64_t>(ask_levels_[i].total_qty)});
            count++;
        }
    }
}

Top5BidsAsks LobEngine::generate_top5_snapshot() const noexcept {
    Top5BidsAsks snap;
    std::memset(&snap, 0, sizeof(Top5BidsAsks));
    snap.anchor_price_low32 = static_cast<uint32_t>(anchor_price_ & 0xFFFFFFFF);

    int count = 0;
    for (int32_t i = best_bid_index_; i >= 0 && count < 5; --i) {
        if (bid_levels_[i].total_qty > 0) {
            snap.bid_price_offsets[count] = static_cast<int16_t>(i - static_cast<int32_t>(kMaxPriceLevels / 2));
            snap.bid_qtys[count] = bid_levels_[i].total_qty;
            count++;
        }
    }

    count = 0;
    for (int32_t i = best_ask_index_; i < static_cast<int32_t>(kMaxPriceLevels) && count < 5; ++i) {
        if (ask_levels_[i].total_qty > 0) {
            snap.ask_price_offsets[count] = static_cast<int16_t>(i - static_cast<int32_t>(kMaxPriceLevels / 2));
            snap.ask_qtys[count] = ask_levels_[i].total_qty;
            count++;
        }
    }

    return snap;
}

size_t LobEngine::snapshot_into(uint8_t* buf, size_t buf_len) const noexcept {
    if (buf_len < sizeof(LobInternalSnapshot)) return 0;

    uint32_t active_orders = 0;
    for (size_t i = 0; i < kMaxPriceLevels; ++i) {
        auto* curr = bid_levels_[i].head;
        while (curr) { active_orders++; curr = curr->next; }
        curr = ask_levels_[i].head;
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
        // Bids
        auto* curr = bid_levels_[i].head;
        while (curr) {
            egbos::core::SerializedOrder so{};
            so.order_id    = curr->order_id;
            so.qty         = curr->qty;
            so.price_index = curr->price_index;
            so.is_bid      = 1;
            so._pad[0] = 0; so._pad[1] = 0; so._pad[2] = 0;
            std::memcpy(buf + offset, &so, sizeof(so));
            offset += sizeof(so);
            num_in_level++;
            curr = curr->next;
        }
        // Asks
        curr = ask_levels_[i].head;
        while (curr) {
            egbos::core::SerializedOrder so{};
            so.order_id    = curr->order_id;
            so.qty         = curr->qty;
            so.price_index = curr->price_index;
            so.is_bid      = 0;
            so._pad[0] = 0; so._pad[1] = 0; so._pad[2] = 0;
            std::memcpy(buf + offset, &so, sizeof(so));
            offset += sizeof(so);
            num_in_level++;
            curr = curr->next;
        }
        
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

    for (uint32_t i = 0; i < hdr.order_count; ++i) {
        egbos::core::SerializedOrder so;
        std::memcpy(&so, buf + offset + i * sizeof(so), sizeof(so));
        if (so.price_index < 0 || so.price_index >= static_cast<int32_t>(kMaxPriceLevels)) {
            return false;
        }
    }

    for (auto& level : bid_levels_) {
        level.head      = nullptr;
        level.tail      = nullptr;
        level.total_qty = 0;
    }
    for (auto& level : ask_levels_) {
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
        node->is_bid      = so.is_bid;
        node->next        = nullptr;
        node->prev        = nullptr;

        link_node(so.price_index, node, so.is_bid);

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
