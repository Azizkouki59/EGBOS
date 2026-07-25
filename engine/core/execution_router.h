// engine/core/execution_router.h
// EGBOS L5 Execution Router
//
// Specification : EGBOS Master Blueprint S4, §E1.2 Execution Module Registry
// ADR compliance: AD-011 (ONNX-only model inference in L5)
//                 AD-009 (FlatBuffers for inter-layer event types)
//                 AD-004 (fixed-point arithmetic, PRICE_SCALE = 100,000)
//                 AD-006 (SHM sizes compile-time constants)
// Claims        : CLAIM-L3-02 (V3) — FIX consumer zero business logic
//                 CLAIM-L3-05 (V3) — startup buffer empirically sized
// Govcheck      : GC-E-042  — zero business logic in FIX consumer
//                 GC-E-RESEARCH-003 — no pickle/torch/joblib/SavedModel in L5
//                 GC-E-010  — no malloc/new/delete/free on hot path
//                 GC-E-011  — no CLOCK_REALTIME
//
// Forbidden     : pickle, torch.load, joblib, tf.SavedModel (AD-011)
//                 Any business logic in the routing loop (pure translation)
//                 Runtime memory allocation on the hot path (AD-006)
//                 Floating-point arithmetic (AD-004)

#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>

#include "shm_layout.h"

namespace egbos::l5 {

// ── ExecCommand ──────────────────────────────────────────────────────────────
// 64-byte slot read from egbos_exec_commands SPSC ring (L3 → L5).
// Layout must match the producer side in position_engine.h.
// Spec §6.2: "SPSC ring, 4096 slots × 64B"
struct alignas(64) ExecCommand {
    uint64_t session_id;      // FIX SessionID (GC-E-042 — must be set by L3)
    uint64_t msg_seq_num;     // FIX MsgSeqNum (GC-E-042 — must be set by L3)
    int64_t  price;           // Fixed-point, PRICE_SCALE = 100,000 (AD-004)
    int64_t  quantity;        // Fixed-point
    uint8_t  r_applied;       // R_applied flag (GC-E-042 — must be set by L3)
    uint8_t  side;            // 0=buy, 1=sell
    uint8_t  _reserved[38];   // Pad to 64 bytes
};
static_assert(sizeof(ExecCommand) == 64, "ExecCommand must be exactly 64 bytes");

// ── ExecutionRouter ───────────────────────────────────────────────────────────
// L5: Single-consumer reader from egbos_exec_commands SPSC.
// Responsibilities:
//   1. Drain the SPSC ring without blocking.
//   2. Forward commands to FIX consumer for wire encoding (zero business logic).
//   3. Check governance halt latch before every dispatch.
//   4. Record command count for CLAIM-L3-05 startup buffer sizing.
//
// This class does NOT allocate memory on the hot path (GC-E-010).
// This class does NOT use ONNX Runtime directly — model inference is not
// performed in the router; only order routing is performed here.
// ONNX loading is deferred to the signal evaluation layer (not yet S4 scope).
class ExecutionRouter {
public:
    // Construct from mapped SHM region pointers (obtained from shm_init_all).
    // Does NOT call malloc/new.
    explicit ExecutionRouter(
        shm::ExecCommandsRegion*  exec_ring,
        const shm::GovernanceFlags* gov_flags
    ) noexcept;

    // Poll one command from the SPSC ring.
    // Returns true if a command was processed, false if the ring was empty.
    // Checks halt_required before processing. If halted, returns false.
    // Does NOT allocate (GC-E-010). Does NOT use CLOCK_REALTIME (GC-E-011).
    [[nodiscard]] bool poll_one() noexcept;

    // Returns the total number of commands consumed since construction.
    // Used by CLAIM-L3-05 startup buffer analysis.
    [[nodiscard]] uint64_t commands_consumed() const noexcept {
        return commands_consumed_.load(std::memory_order_relaxed);
    }

    // Returns true if the governance halt latch is set.
    [[nodiscard]] bool is_halted() const noexcept;

private:
    shm::ExecCommandsRegion*    exec_ring_;
    const shm::GovernanceFlags* gov_flags_;
    std::atomic<uint64_t>       commands_consumed_{0};
};

// ── ExecCommandsRegion alias ──────────────────────────────────────────────────
// Alias for the SPSC ring type defined in shm_layout.h, for use by L5 code.
struct ExecCommandSlot {
    alignas(64) uint8_t data[shm::kExecCommandsSlotSize];
};
using ExecCommandsRegion = egbos::core::SpscRing<ExecCommandSlot, shm::kExecCommandsSlots>;

}  // namespace egbos::l5
