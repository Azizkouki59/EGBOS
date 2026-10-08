// engine/l2/feature_reader.h
// EGBOS — L2 → L3 Feature Reader
//
// Reader-side API for the per-instrument FeatureRegion ABI.
// Wraps FeatureSlot::try_read() behind a bounds-checked interface so L3
// DecisionEngine does not manipulate raw SHM pointers directly.
//
// Threading: multiple readers are safe (seqlock read-side is wait-free for
// any single reader; concurrent readers of independent slots do not interfere).
// No dynamic allocation.  No floating-point.
//
// Usage:
//   egbos::l2::FeatureReader reader(region);
//
//   egbos::l2::FeatureSlotView view;
//   if (reader.try_read(instrument_id, view)) {
//       // view fields are consistent — use view.payload[0..view.payload_bytes-1]
//   }
//
// Schema version rejection:
//   FeatureReader::try_read() returns false when the slot's schema_version
//   differs from kFeatureSlotSchemaVersion.  L3 must never accept unknown ABI.

#pragma once

#include "l2/feature_abi.h"         // kFeatureSlotSchemaVersion
#include "core/shm_layout.h"
#include <cstdint>
#include <cstring>

namespace egbos::l2 {

// FeatureSlotView — caller-owned copy of a consistent FeatureSlot snapshot.
// Filled by FeatureReader::try_read() on success.
// payload[0..payload_bytes-1] contains meaningful data when validity_mask != 0.
struct FeatureSlotView {
    uint32_t instrument_id{0};
    uint8_t  schema_version{0};
    uint16_t payload_count{0};
    uint64_t source_seq{0};
    uint64_t pub_ts_ns{0};
    uint64_t validity_mask{0};
    uint32_t payload_bytes{0};
    uint8_t  payload[egbos::shm::kFeatureSlotPayloadBytes]{};
};

static_assert(std::is_standard_layout_v<FeatureSlotView>,
    "FeatureSlotView must be standard-layout");

class FeatureReader {
public:
    // region must not be null.
    explicit FeatureReader(const egbos::shm::FeatureRegion* region) noexcept
        : region_(region) {}

    // Try to read a consistent snapshot of the given instrument's feature slot.
    // Returns false if:
    //   - region_ is null
    //   - instrument_id >= kMaxInstruments
    //   - slot was never published (seq == 0)
    //   - write in progress (seq odd) — non-spinning; caller retries
    //   - torn read (seq changed during copy)
    //   - schema_version != kFeatureSlotSchemaVersion (ABI mismatch)
    //   - instrument_id in slot != expected instrument_id (aliasing bug)
    //
    // On success all fields of out are populated from a consistent snapshot.
    // Does NOT call malloc/new (GC-E-010).
    [[nodiscard]] bool try_read(
        uint32_t instrument_id,
        FeatureSlotView& out) const noexcept
    {
        if (!region_) return false;
        if (instrument_id >= egbos::shm::kMaxInstruments) return false;

        const bool ok = region_->slots[instrument_id].try_read(
            instrument_id,
            out.instrument_id,
            out.schema_version,
            out.source_seq,
            out.pub_ts_ns,
            out.validity_mask,
            out.payload_count,
            out.payload_bytes,
            out.payload);

        if (!ok) return false;

        // Reject unsupported schema versions — L3 must not process unknown ABI.
        if (out.schema_version != kFeatureSlotSchemaVersion) return false;

        return true;
    }

private:
    const egbos::shm::FeatureRegion* region_;
};

} // namespace egbos::l2
