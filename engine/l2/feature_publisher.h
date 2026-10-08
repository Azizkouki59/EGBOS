// engine/l2/feature_publisher.h
// EGBOS — L2 → L3 Feature Publisher
//
// Writer-side API for the per-instrument FeatureRegion ABI.
// Wraps FeatureRegion::slots[instrument_id].publish() behind a bounds-checked
// interface so FeatureEngine does not manipulate raw SHM pointers directly.
//
// Threading: one writer per instrument slot (SPSC-SINGLE guaranteed at layer level).
// No dynamic allocation.  No floating-point for prices/quantities.
// Spec §3b: per-instrument seqlock protocol enforced by FeatureSlot::publish().
//
// GOVERNANCE NOTICE:
//   This publisher sets schema_version=1 / kFeatureSlotSchemaVersion and validity_mask=0 until EGBOS
//   approves a feature contract.  Do NOT populate payload bytes without a
//   ratified feature contract (feature_id, version, exact formula, etc.)

#pragma once

#include "core/shm_layout.h"
#include "l2/feature_abi.h"
#include <cstdint>

namespace egbos::l2 {

class FeaturePublisher {
public:
    // region must not be null.
    explicit FeaturePublisher(egbos::shm::FeatureRegion* region) noexcept
        : region_(region) {}

    // Publish a skeleton slot for the given instrument.
    // Returns false if region_ is null or instrument_id >= kMaxInstruments.
    //
    // source_seq   : the L1 snapshot sequence number that triggered this publish.
    // pub_ts_ns    : caller-supplied timestamp (CLOCK_MONOTONIC_RAW, nanoseconds).
    // validity_mask: bit N=1 ↔ feature N in payload is valid.
    //                Pass 0 until EGBOS approves the first feature contract.
    // src_payload  : pointer to payload bytes (may be nullptr if bytes==0).
    // src_bytes    : number of meaningful payload bytes
    //                (≤ egbos::shm::kFeatureSlotPayloadBytes).
    // feature_count: number of valid feature entries in payload (may be 0).
    //
    // Does NOT call malloc/new (GC-E-010).
    bool publish(
        uint32_t        instrument_id,
        uint64_t        source_seq,
        uint64_t        pub_ts_ns,
        uint64_t        validity_mask,
        uint16_t        feature_count,
        const uint8_t*  src_payload,
        uint32_t        src_bytes) noexcept
    {
        if (!region_) return false;
        if (instrument_id >= egbos::shm::kMaxInstruments) return false;

        return region_->slots[instrument_id].publish(
            instrument_id,
            kFeatureSlotSchemaVersion,
            source_seq,
            pub_ts_ns,
            validity_mask,
            feature_count,
            src_payload,
            src_bytes);
    }

private:
    egbos::shm::FeatureRegion* region_;
};

} // namespace egbos::l2
