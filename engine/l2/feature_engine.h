// engine/l2/feature_engine.h
// EGBOS — L2 Feature Engine (Transport Skeleton)
//
// Processes L1 Top5BidsAsks snapshots and produces L3 FeatureSlots.
//
// Output target: per-instrument FeatureRegion (shm::FeatureRegion) via
// FeaturePublisher.  One seqlock-protected FeatureSlot per instrument.
//
// GOVERNANCE NOTICE:
//   This engine currently implements ONLY the L1→L2 read path and the
//   L2→L3 transport skeleton (schema-version + validity-mask write). The
//   feature payload is left in a documented zero/invalid state. No feature
//   mathematics has been approved by EGBOS, and none is implemented here.
//
// BLOCKED: All price-reconstruction logic — see current unresolved governance decisions (§PRICE-RECON).
// BLOCKED: All approved feature values — no EGBOS feature contract exists yet.
//
// ABI assertions for Top5BidsAsks (repository ABI/schema contract, corrected layout):
//   anchor_price_low32     uint32_t  @ offset 60
//   bid_price_offsets[5]   int16_t   @ offset 40
//   ask_price_offsets[5]   int16_t   @ offset 50
//   bid_qtys[5]            uint32_t  @ offset  0
//   ask_qtys[5]            uint32_t  @ offset 20
// These are read by LobSnapshotReader but anchor+offsets are NOT decoded:
// price reconstruction semantics are unresolved (see current unresolved governance decisions §PRICE-RECON).

#pragma once

#include "l2/lob_snapshot_reader.h"
#include "l2/feature_publisher.h"
#include "core/shm_layout.h"

namespace egbos::l2 {

class FeatureEngine {
public:
    // lob_region: source of L1 snapshots (may be null — handled gracefully).
    // feature_region: destination per-instrument FeatureRegion (must not be null).
    explicit FeatureEngine(
        const egbos::shm::LobSnapshotsRegion* lob_region,
        egbos::shm::FeatureRegion*             feature_region) noexcept;

    // Reads the latest snapshot for the given instrument, writes a schema-
    // versioned FeatureSlot with validity_mask=0 and zeroed feature payload.
    // Returns true if the snapshot read succeeded and the slot was published.
    //
    // BLOCKED: Feature payload population — no EGBOS feature contract exists.
    //          See current unresolved governance decisions for enumeration of
    //          every blocked decision.
    bool process_instrument(uint32_t instrument_id) noexcept;

private:
    LobSnapshotReader reader_;
    FeaturePublisher  publisher_;
};

} // namespace egbos::l2
