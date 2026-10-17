// engine/l1/lob_snapshot_publisher.h
// EGBOS — L1 -> L2 LOB snapshot publisher
//
// Scope:
//   Publishes the current L1 Top5 snapshot into the existing
//   /dev/shm/egbos_lob_snapshots double-buffer region.
//
// No feature mathematics is defined here.

#pragma once

#include "engine/core/shm_layout.h"
#include "engine/core/lob_snapshot_abi.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace egbos::l1 {

static_assert(sizeof(Top5BidsAsks) == 128,
              "L1->L2 payload ABI must remain 128 bytes (v2)");
static_assert(alignof(Top5BidsAsks) == 64,
              "L1->L2 payload ABI must remain 64-byte aligned");

static_assert(offsetof(Top5BidsAsks, bid_qtys)          ==   0);
static_assert(offsetof(Top5BidsAsks, ask_qtys)          ==  20);
static_assert(offsetof(Top5BidsAsks, bid_price_offsets) ==  40);
static_assert(offsetof(Top5BidsAsks, ask_price_offsets) ==  50);
static_assert(offsetof(Top5BidsAsks, anchor_price)      ==  64);
static_assert(offsetof(Top5BidsAsks, tick_size)         ==  72);

class LobSnapshotPublisher {
public:
    explicit LobSnapshotPublisher(
        egbos::shm::LobSnapshotsRegion* region) noexcept
        : region_(region) {}

    // Single L1 writer per instrument.
    // Writes the inactive buffer completely before publishing the new
    // active-buffer index.
    //
    // Only the first 64 payload bytes are currently defined by the
    // implementation ABI. Remaining bytes are intentionally unspecified.
    bool publish(
        uint32_t instrument_id,
        const Top5BidsAsks& snapshot) noexcept
    {
        if (region_ == nullptr || instrument_id >= egbos::shm::kMaxInstruments) {
            return false;
        }

        auto& instrument = region_->instruments[instrument_id];

        const uint8_t active =
            instrument.header.active_buffer.load(
                std::memory_order_acquire);

        if (active >= egbos::shm::kLobSnapshotBuffers) {
            return false;
        }

        const uint8_t inactive = static_cast<uint8_t>(active ^ 1u);
        auto& buffer = instrument.buffers[inactive];

        // Establish an odd sequence regardless of stale state left by a
        // previously interrupted writer.
        uint64_t seq =
            buffer.sequence.load(std::memory_order_relaxed);

        if (seq & 1u) {
            ++seq;
        }

        buffer.sequence.store(
            seq + 1u,
            std::memory_order_seq_cst);

        std::memcpy(
            buffer.data.payload,
            &snapshot,
            sizeof(Top5BidsAsks));

        std::atomic_thread_fence(std::memory_order_seq_cst);

        buffer.sequence.store(
            seq + 2u,
            std::memory_order_seq_cst);

        // Release-publish the completed buffer only after the payload and
        // sequence are complete.
        instrument.header.active_buffer.store(
            inactive,
            std::memory_order_release);

        return true;
    }

private:
    egbos::shm::LobSnapshotsRegion* region_;
};

} // namespace egbos::l1
