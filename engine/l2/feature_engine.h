// engine/l2/feature_engine.h
// EGBOS — L2 Feature Engine (Transport Skeleton)
//
// Processes L1 Top5BidsAsks snapshots and produces L3 FeatureVectors.
// Currently implements only the L1 -> L2 -> L3 transport boundary,
// as quantitative feature formulas are awaiting an authoritative specification.

#pragma once

#include "engine/l2/lob_snapshot_reader.h"
#include "engine/core/shm_layout.h"

namespace egbos::l2 {

class FeatureEngine {
public:
    // feature_region must not be null. lob_region can be null (handled by reader).
    explicit FeatureEngine(
        const egbos::shm::LobSnapshotsRegion* lob_region,
        egbos::shm::FeatureVectorRegion* feature_region) noexcept;

    // Reads the latest snapshot for the given instrument and forwards a computed
    // feature vector to L3. Returns true if a snapshot was successfully read.
    bool process_instrument(uint32_t instrument_id) noexcept;

private:
    LobSnapshotReader reader_;
    egbos::shm::FeatureVectorRegion* feature_region_;
};

} // namespace egbos::l2
