// engine/l2/candidates/candidates_common.h
// EGBOS — L2 Candidate Feature Module: common types and status enum
//
// ============================================================
//  GOVERNANCE NOTICE — READ BEFORE USING ANYTHING IN THIS DIR
// ============================================================
//
// Everything in engine/l2/candidates/ is COMPUTABLE CANDIDATE code.
// "Computable candidate" means: the formula can be evaluated from the
// current ABI inputs, but it is NOT an EGBOS-approved production feature.
//
// No function in this directory may be called from the production
// FeatureEngine::process_instrument() path.
//
// No function in this directory may populate the FeatureVector payload.
//
// No feature_id has been assigned to any function in this directory.
//
// When EGBOS approves a feature contract for one of these functions,
// the contract will:
//   (1) assign a feature_id
//   (2) designate the exact formula (possibly with modifications)
//   (3) reference the approved parity test
//   (4) move the implementation to the production FeatureEngine
//
// Source authority: research-only quantitative reference — NOT EGBOS-SPECIFIED (quantity-only computable
// candidates), §5.1 (CKS OFI pure formula), §9 (parity governance).
//
// Label carried by every function in this module:
//   // COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
//   // (see research-only quantitative reference — NOT EGBOS-SPECIFIED)

#pragma once

#include <cstdint>

namespace egbos::l2::candidates {

// ── ComputeStatus ──────────────────────────────────────────────────────────
//
// Every candidate function that can encounter a denominator-zero or other
// undefined condition MUST return this status alongside its value.
// Silent fallback to zero without signaling invalidity is prohibited.
//
// Canonical spec §8.1: "denominator-zero behaviour exactly as the spec
// leaves it (unresolved) — implement an explicit validity flag output,
// never a silent fallback to zero without signaling invalidity."
//
// current unresolved governance decision §DENOM-ZERO: the exact policy (return NaN, clamp to 0,
// treat as "missing", propagate to validity mask) is unresolved by EGBOS.
// Until resolved, callers must treat INVALID results as unusable.
enum class ComputeStatus : uint8_t {
    OK      = 0,  // result is mathematically valid
    INVALID = 1,  // denominator was zero or input was degenerate; value undefined
};

// ── Convenience: result-with-status pair ──────────────────────────────────
//
// Returned by every candidate function that can produce INVALID results.
// Never discard the .status field; always check it before using .value.
template <typename T>
struct Result {
    T            value{};
    ComputeStatus status{ComputeStatus::INVALID};

    [[nodiscard]] bool ok() const noexcept { return status == ComputeStatus::OK; }
};

// Number of levels in the Top5BidsAsks ABI.
// Canonical spec §2: exactly 5 bid levels and 5 ask levels.
inline constexpr unsigned kLevels = 5u;

} // namespace egbos::l2::candidates
