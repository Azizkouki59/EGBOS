// engine/l2/feature_engine.h
// EGBOS — L2 FeatureEngine (production framework, contract-driven)
//
// Orchestrates the per-instrument feature production pipeline:
//
//   L1 LobSnapshotRegion
//       ↓  LobSnapshotReader    (seqlock read, source_seq surfaced)
//   snapshot validation
//       ↓  deduplication        (skip if source_seq unchanged)
//       ↓  gap detection        (reset Hawkes state on seq jump > +2)
//   compute_production_features()   ← single production computation entry point
//       ↓                            (feature_compute.h / feature_compute.cpp)
//   FeaturePublisher                ← per-instrument seqlock write
//       ↓
//   FeatureRegion (one slot per instrument)
//
// DESIGN: CONTRACT-DRIVEN ISOLATION
//   Feature computation is structurally decoupled from transport:
//     - engine/l2/feature_compute.h   defines the FeatureComputeOutput struct
//                                     and the compute_production_features() API.
//     - engine/l2/feature_compute.cpp contains the sole production implementation.
//     - engine/l2/candidates/         is research-only; FeatureEngine MUST NOT
//                                     include or call any candidate module.
//   This structure makes it impossible for research candidates to enter the
//   production path without explicit implementation of the compute interface.
//
// CLOCK POLICY
//   now_monotonic_ns() is called exactly once per process_instrument() tick.
//   The result is forwarded to compute_production_features() as pub_ts_ns.
//   The compute layer MUST NOT query the clock independently.
//
// RATIFIED FEATURES (as of this commit)
//   FEAT-0001: Hawkes Intensity (schema_version=1 / kFeatureSlotSchemaVersion)
//     validity_mask bit 0 set, feature_count=1, payload_bytes=4.
//     Governance: governance/claims/FEAT-0001-Hawkes-Intensity.md
//     Parameters: governance/claims/PARAMETER_REGISTRY
//
// ABI assertions for Top5BidsAsks (repository ABI/schema contract, v2):
//   bid_qtys[5]            uint32_t[5] @ offset  0
//   ask_qtys[5]            uint32_t[5] @ offset 20
//   bid_price_offsets[5]   int16_t[5]  @ offset 40
//   ask_price_offsets[5]   int16_t[5]  @ offset 50
//   anchor_price           int64_t     @ offset 64  (§PRICE-RECON resolved)
//   tick_size              int64_t     @ offset 72  (§PRICE-RECON resolved)
// Use snap.calculate_absolute_price(level_offset) for overflow-safe reconstruction.

#pragma once

#include "l2/lob_snapshot_reader.h"
#include "l2/feature_publisher.h"
#include "l2/feature_compute.h"
#include "core/shm_layout.h"

#include <cstdint>

namespace egbos::l2 {

class FeatureEngine {
public:
    // lob_region: source of L1 snapshots (may be null — handled gracefully).
    // feature_region: destination per-instrument FeatureRegion (must not be null).
    explicit FeatureEngine(
        const egbos::shm::LobSnapshotsRegion* lob_region,
        egbos::shm::FeatureRegion*             feature_region) noexcept;

    // ── process_instrument ─────────────────────────────────────────────────
    //
    // Reads the latest L1 snapshot for instrument_id, runs the production
    // feature computation interface, and publishes the result as a FeatureSlot.
    //
    // Returns true if:
    //   - The L1 snapshot was read successfully (source_seq > 0).
    //   - The snapshot represents a NEW sequence (dedup: skips re-computation
    //     if source_seq is unchanged since the last call).
    //   - The compute step succeeded.
    //   - The FeatureSlot was published without payload overflow.
    //
    // Returns false if:
    //   - lob_region or feature_region is null.
    //   - instrument_id is out of range.
    //   - No L1 snapshot has been published for this instrument.
    //   - source_seq is unchanged (dedup — not an error, just a no-op).
    //   - The compute step returned false.
    //   - Publication was rejected (oversized payload).
    [[nodiscard]] bool process_instrument(uint32_t instrument_id) noexcept;

    // ── process_all_published ──────────────────────────────────────────────
    //
    // Iterates [0, kMaxInstruments) and calls process_instrument() for each.
    // This is the production hot-path entry point for a full-sweep processing
    // tick.  Instruments for which no L1 snapshot has been published, or whose
    // sequence has not changed, are silently skipped.
    //
    // Returns the number of instruments whose FeatureSlot was successfully
    // updated (i.e. new data computed and published) in this sweep.
    //
    // Does NOT allocate memory.  Does NOT throw.
    // No background threads are introduced here.
    uint32_t process_all_published() noexcept;

private:
    LobSnapshotReader reader_;
    FeaturePublisher  publisher_;

    // Per-instrument last-seen source_seq for deduplication.
    // Initialised to 0 in the constructor (0 == never processed).
    // A new source_seq equal to the stored value causes process_instrument()
    // to return false without re-computing or re-publishing.
    uint64_t last_source_seq_[egbos::shm::kMaxInstruments];

    // Monotonic clock query, isolated so it can be replaced without touching
    // production logic.  Called exactly once per process_instrument() tick.
    [[nodiscard]] static uint64_t now_monotonic_ns() noexcept;
};

} // namespace egbos::l2
