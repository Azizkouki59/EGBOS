// engine/core/fix_consumer.h
// EGBOS FIX Consumer — Pure Translation Layer
//
// Specification : EGBOS Master Blueprint S4, §E1.2
// ADR compliance: AD-009 (FlatBuffers for inter-layer event types)
// Claims        : CLAIM-L3-02 (V3) — zero business logic in FIX consumer
// Govcheck      : GC-E-042 — zero business logic; control flow triggers fatal
//
// CONTRACT (per CLAIM-L3-02 / GC-E-042):
//   This module is a PURE TRANSLATION LAYER.
//   It encodes ExecCommand structs into FIX 4.2 wire bytes.
//   It performs ZERO order evaluation, ZERO risk checking, ZERO decision making.
//   It does NOT reject, modify, prioritize, or defer commands.
//   All such responsibilities are EXCLUSIVELY in L3 (PositionEngine) and L4 (RiskEnforcer).
//
// Required FIX fields per GC-E-042:
//   SessionID   — must be set by L3 before pushing to SPSC
//   MsgSeqNum   — must be set by L3 before pushing to SPSC
//   R_applied   — must be set by L3 before pushing to SPSC
//
// Forbidden:
//   Any if/switch/while/for on command content (GC-E-042)
//   Any inference framework: pickle, torch.load, joblib, SavedModel (GC-E-RESEARCH-003)
//   Any malloc/new/delete/free (GC-E-010)
//   Any CLOCK_REALTIME (GC-E-011)
//   Any floating-point arithmetic (AD-004)

#pragma once

#include <cstdint>
#include <cstddef>
#include "execution_router.h"

namespace egbos::l5 {

// FIX 4.2 encoded output buffer.
// Fixed-size stack allocation — no heap involvement (GC-E-010).
inline constexpr std::size_t kFixMsgMaxBytes = 512;

struct FixEncodedMsg {
    uint8_t  data[kFixMsgMaxBytes]{};
    uint32_t length{0};   // Populated by encode()
};

// ── FIXConsumer ───────────────────────────────────────────────────────────────
// Pure translation: ExecCommand → FIX 4.2 wire bytes.
// No business logic. No allocation. No evaluation.
// Compliant with: GC-E-042, GC-E-RESEARCH-003, GC-E-010, GC-E-011, AD-009.
class FIXConsumer {
public:
    FIXConsumer() noexcept = default;

    // Encode one ExecCommand into FIX 4.2 wire format.
    // Pure field mapping: cmd → FIX tags 35, 49, 56, 34, 38, 40, 44, 54.
    // Zero control flow on command content — translation only (GC-E-042).
    // Does NOT call malloc/new (GC-E-010).
    // Does NOT use CLOCK_REALTIME (GC-E-011).
    // Does NOT use floating-point (AD-004).
    // Returns true on success, false if output buffer is too small.
    [[nodiscard]] bool encode(const ExecCommand& cmd, FixEncodedMsg& out) noexcept;

    // Returns total messages encoded since construction.
    [[nodiscard]] uint64_t messages_encoded() const noexcept {
        return messages_encoded_;
    }

private:
    uint64_t messages_encoded_{0};
};

}  // namespace egbos::l5
