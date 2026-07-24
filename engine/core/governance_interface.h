// engine/core/governance_interface.h
// EGBOS ENGINE_V3 — Governance Interface
//
// Specification  : EGBOS-SPEC-S3-001 §5.3

#pragma once

#include <atomic>
#include <cstdint>

namespace egbos::core {

// Read-only access to governance flags SHM region
// Called once per operational loop iteration (E2)
// Returns: GovernanceFlags snapshot — not cached between iterations
struct GovernanceFlags {
  std::atomic<uint8_t> halt_required;    // Set by L4 Safety Monitors
  std::atomic<uint8_t> feed_stale;       // Set by C1 Gap Detection
  std::atomic<uint8_t> preflight_armed;  // Set by E3 Pre-flight Protocol
  std::atomic<int64_t> pos_limit_latch;  // Monotone — only decreases (AD-010)
  uint8_t              _pad[48];         // Pad to 64 bytes — one cache line
};

static_assert(sizeof(GovernanceFlags) == 64, "GovernanceFlags must be exactly 64 bytes");

} // namespace egbos::core
