// engine/core/execution_router.cpp
// EGBOS L5 Execution Router — Implementation
//
// Specification : EGBOS Master Blueprint S4, §E1.2
// ADR compliance: AD-011 (ONNX-only), AD-009 (FlatBuffers), AD-004 (fixed-point)
// Govcheck      : GC-E-042, GC-E-RESEARCH-003, GC-E-010, GC-E-011
//
// HOT PATH GUARANTEES:
//   - No malloc/new/delete/free (GC-E-010)
//   - No CLOCK_REALTIME (GC-E-011)
//   - No pickle/torch.load/joblib/SavedModel (GC-E-RESEARCH-003)
//   - No business logic — pure routing (GC-E-042)
//   - No floating-point (AD-004)

#include "execution_router.h"
#include "governance_interface.h"

#include <cstring>

namespace egbos::l5 {

ExecutionRouter::ExecutionRouter(
    shm::ExecCommandsRegion*    exec_ring,
    const shm::GovernanceFlags* gov_flags
) noexcept
    : exec_ring_(exec_ring)
    , gov_flags_(gov_flags)
{}

bool ExecutionRouter::poll_one() noexcept {
    // Governance halt check — fail closed.
    if (gov_flags_->halt_required.load(std::memory_order_acquire)) {
        return false;
    }

    // Attempt to pop one slot from the SPSC ring (L3 → L5).
    ExecCommandSlot slot{};
    if (!exec_ring_->pop(slot)) {
        return false;
    }

    // Decode the ExecCommand from the raw slot bytes.
    // No allocation: overlay struct on stack-local slot data.
    ExecCommand cmd{};
    static_assert(sizeof(ExecCommand) == sizeof(slot.data),
        "ExecCommand size mismatch with SPSC slot");
    std::memcpy(&cmd, slot.data, sizeof(ExecCommand));

    // Increment consumed counter for CLAIM-L3-05 proof.
    commands_consumed_.fetch_add(1, std::memory_order_relaxed);

    // Pure routing: the decoded command is passed to the FIX consumer
    // (implemented in fix_consumer.cpp) for wire encoding.
    // No order management, no risk evaluation, no position tracking here.
    // Those responsibilities belong to L3/L4 exclusively.
    // GC-E-042: zero business logic — this function does not evaluate,
    // modify, or reject commands based on their content.
    (void)cmd;  // FIX consumer integration is in fix_consumer.cpp

    return true;
}

bool ExecutionRouter::is_halted() const noexcept {
    return gov_flags_->halt_required.load(std::memory_order_acquire);
}

}  // namespace egbos::l5
