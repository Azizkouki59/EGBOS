// engine/l2/feature_compute.h
// EGBOS — L2 FeatureEngine production computation interface.
//
// This header defines the contract between the FeatureEngine orchestrator
// (snapshot acquisition, validation, publication) and the feature computation
// layer (feature formulas, payload construction).
//
// DESIGN RATIONALE
//   Separating this interface from feature_engine.h ensures that:
//     (a) Research candidate modules in engine/l2/candidates/ can never be
//         included from production code without explicitly implementing this
//         interface.
//     (b) Each future approved feature contract is wired through exactly one
//         registered implementation — there is no implicit promotion path.
//     (c) The FeatureEngine can be fully tested without any feature math.
//
// RATIFIED PRODUCTION FEATURES
//   FEAT-0001: Hawkes Intensity (Feature ID: 0x0001)
//     Contract : governance/claims/FEAT-0001-Hawkes-Intensity.md
//     Registry : governance/claims/PARAMETER_REGISTRY
//     Payload  : 32-bit float occupying bytes 0-3 of the slot payload
//     Validity : bit 0 of validity_mask (0x0001)
//     Formula  : λ(t) = μ + Σ α * exp(-β * (t - t_i))
//       μ (baseline) = 0.1    (source: FEAT-0001-Hawkes-Intensity.md §2)
//       α (jump)     = 0.05   (source: FEAT-0001-Hawkes-Intensity.md §2)
//       β (decay)    = 0.01   (source: FEAT-0001-Hawkes-Intensity.md §2)
//
//   FEAT-0002: Level-1 Order Flow Imbalance (OFI) (Feature ID: 0x0002)
//     Contract : governance/claims/FEAT-0002-Order-Flow-Imbalance.md
//     Registry : governance/claims/PARAMETER_REGISTRY
//     Payload  : 32-bit float occupying bytes 4-7 of the slot payload
//     Validity : bit 1 of validity_mask (0x0002)
//     Formula  : OFI = e_n^b - e_n^a using reconstructed absolute prices
//                Cold start / gap = 0.0f
//
// CLOCK POLICY
//   compute_production_features() MUST NOT query the clock.
//   The FeatureEngine queries CLOCK_MONOTONIC_RAW once per tick via
//   now_monotonic_ns() and passes the result as pub_ts_ns.
//   This keeps the compute layer deterministic and testable.
//
// INCLUDE CONSTRAINTS
//   This header MUST NOT include anything from engine/l2/candidates/.

#pragma once

#include "core/lob_snapshot_abi.h"   // egbos::l1::Top5BidsAsks
#include "core/shm_layout.h"          // kFeatureSlotPayloadBytes
#include <cstdint>

namespace egbos::l2 {

// ── FeatureComputeOutput ────────────────────────────────────────────────────
//
// Carries the feature computation result from the computation layer back
// to the FeatureEngine publisher.
//
// payload_bytes: must be <= egbos::shm::kFeatureSlotPayloadBytes.
//   If src_payload == nullptr, payload_bytes must be 0.
//
// INVARIANT (enforced by FeatureEngine before publish):
//   payload_bytes <= kFeatureSlotPayloadBytes
struct FeatureComputeOutput {
    uint64_t validity_mask  = 0u;     // bit N=1 means feature N in payload is valid
    uint16_t feature_count  = 0u;     // number of feature entries in payload
    uint32_t payload_bytes  = 0u;     // meaningful bytes in payload[]
    // Inline fixed-size payload buffer.  Using inline storage (no pointer)
    // guarantees no dynamic allocation on the hot path (GC-E-010).
    uint8_t  payload[egbos::shm::kFeatureSlotPayloadBytes]{};
};

// ── compute_production_features ────────────────────────────────────────────
//
// The single production entry point for feature computation.
//
// Called by FeatureEngine after a validated L1 snapshot is available for
// 'instrument_id'.  Fills 'out' with all ratified production features for
// this instrument.
//
// CONTRACT
//   - Must not allocate memory (GC-E-010).
//   - Must not query the clock (see CLOCK POLICY above).
//   - Must not call into engine/l2/candidates/ or any research module.
//   - Reconstructs absolute prices using anchor_price and tick_size (§PRICE-RECON).
//   - Populates FEAT-0001 (Hawkes Intensity) into payload bytes 0-3.
//   - Populates FEAT-0002 (Level-1 OFI) into payload bytes 4-7.
//   - Sets validity_mask = 3 (bits 0 and 1), feature_count = 2, payload_bytes = 8.
//   - Must remain noexcept.
//
// instrument_id : instrument being processed
// snapshot      : validated L1 snapshot for instrument_id
// pub_ts_ns     : monotonic timestamp (ns) queried once by the caller
//                 (CLOCK_MONOTONIC_RAW via FeatureEngine::now_monotonic_ns()).
//                 Passed in to keep this layer clock-free and testable.
// out           : output buffer to fill — pre-zeroed by caller
//
// Returns true on success.
[[nodiscard]] bool compute_production_features(
    uint32_t                          instrument_id,
    const egbos::l1::Top5BidsAsks&   snapshot,
    uint64_t                          pub_ts_ns,
    FeatureComputeOutput&             out) noexcept;

// Reset per-instrument production feature state (e.g. for testing or gap handling).
void reset_production_features(uint32_t instrument_id) noexcept;
void reset_all_production_features() noexcept;

} // namespace egbos::l2
