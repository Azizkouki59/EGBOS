// engine/l2/lob_snapshot_reader.h
// EGBOS — L2 LOB snapshot reader
//
// Scope:
//   L1 -> L2 SHM transport only.
//   No feature mathematics is defined here.
//
// Current implementation payload:
//   Top5BidsAsks, 128 bytes (ABI v2) at payload offset 0.
//   v2 adds anchor_price (int64_t) and tick_size (int64_t), resolving §PRICE-RECON.

#pragma once

#include "engine/core/shm_layout.h"
#include "engine/core/lob_snapshot_abi.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace egbos::l2 {

static_assert(sizeof(egbos::l1::Top5BidsAsks) == 128,
              "L1->L2 Top5BidsAsks ABI must be 128 bytes (v2)");
static_assert(alignof(egbos::l1::Top5BidsAsks) == 64,
              "L1->L2 Top5BidsAsks ABI must remain cache-line aligned");

static_assert(offsetof(egbos::l1::Top5BidsAsks, bid_qtys)          ==   0);
static_assert(offsetof(egbos::l1::Top5BidsAsks, ask_qtys)          ==  20);
static_assert(offsetof(egbos::l1::Top5BidsAsks, bid_price_offsets) ==  40);
static_assert(offsetof(egbos::l1::Top5BidsAsks, ask_price_offsets) ==  50);
static_assert(offsetof(egbos::l1::Top5BidsAsks, anchor_price)      ==  64);
static_assert(offsetof(egbos::l1::Top5BidsAsks, tick_size)         ==  72);

static_assert(sizeof(egbos::shm::LobSnapshotBuffer) ==
              egbos::shm::kLobSnapshotPerBufferBytes,
              "LOB snapshot buffer must remain exactly one page");
static_assert(sizeof(egbos::shm::LobSnapshotInstrumentHeader) ==
              egbos::shm::kPageSize,
              "LOB snapshot header must remain exactly one page");
static_assert(sizeof(egbos::shm::LobSnapshotInstrument) ==
              3 * egbos::shm::kPageSize,
              "LOB snapshot instrument ABI drift");
static_assert(sizeof(egbos::shm::LobSnapshotsRegion) ==
              egbos::shm::kLobSnapshotsRegionSize,
              "LOB snapshot region ABI drift");

class LobSnapshotReader {
public:
    explicit LobSnapshotReader(
        const egbos::shm::LobSnapshotsRegion* region) noexcept
        : region_(region) {}

    // Reads one coherent active-buffer snapshot.
    //
    // Reader contract (7 steps):
    //   1. acquire active buffer index
    //   2. acquire sequence from that buffer
    //   3. reject odd sequence (write in progress — retry)
    //   4. copy exactly 64 defined payload bytes
    //   5. acquire fence
    //   6. re-check sequence — must equal seq from step 2
    //   7. re-check active buffer — must equal buffer from step 1
    //   Accept only when both step 6 and step 7 match.
    //
    // No dynamic allocation, clock, PRNG, or external I/O.
    bool read(
        uint32_t instrument_id,
        egbos::l1::Top5BidsAsks& out,
        uint64_t& out_seq) const noexcept
    {
        if (region_ == nullptr || instrument_id >= egbos::shm::kMaxInstruments) {
            return false;
        }

        const auto& instrument = region_->instruments[instrument_id];

        for (;;) {
            const uint8_t active =
                instrument.header.active_buffer.load(
                    std::memory_order_acquire);

            if (active >= egbos::shm::kLobSnapshotBuffers) {
                continue;
            }

            const auto& buffer = instrument.buffers[active];

            const uint64_t seq1 =
                buffer.sequence.load(std::memory_order_acquire);

            if (seq1 == 0) {
                return false;
            }

            if (seq1 & 1u) {
                continue;
            }

            std::memcpy(
                &out,
                buffer.data.payload,
                sizeof(egbos::l1::Top5BidsAsks));

            std::atomic_thread_fence(std::memory_order_acquire);

            const uint64_t seq2 =
                buffer.sequence.load(std::memory_order_acquire);

            const uint8_t active2 =
                instrument.header.active_buffer.load(
                    std::memory_order_acquire);

            if (seq1 == seq2 &&
                !(seq2 & 1u) &&
                active == active2) {
                out_seq = seq1;
                return true;
            }
        }
    }

private:
    const egbos::shm::LobSnapshotsRegion* region_;
};

} // namespace egbos::l2
