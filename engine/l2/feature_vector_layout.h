// engine/l2/feature_vector_layout.h
// EGBOS — L2 FeatureVector layout (schema skeleton)
//
// STATUS: PROVISIONAL LAYOUT — NOT EGBOS-APPROVED
//
// This header defines the byte layout of the 256-byte FeatureVector
// written by the L2 FeatureEngine into /dev/shm/egbos_feature_vector.
//
// GOVERNANCE NOTICE:
//   The layout below is a working transport skeleton (RFC-003). No ratified
//   EGBOS layout specification exists yet. No feature_id has been assigned
//   to any field. No field below represents an EGBOS-approved production
//   feature. Fields are present solely to establish the transport skeleton
//   and make field gaps visible, not to ship feature values.
//
// NO-INVENTION BOUNDARY (RFC-003):
//   - No price-reconstruction logic may be added anywhere in this file.
//   - No feature_id may be assigned to any field without an explicit EGBOS
//     feature contract (a governance/claims/*.yaml entry: feature_id, version,
//     exact formula, window semantics, edge cases, numerical semantics,
//     online/offline references, parity ref).
//   - All payload fields other than schema_version and validity_mask_lo64
//     must remain zeroed / invalid until a feature contract exists and is
//     referenced here by claim_id.

#pragma once

#include <cstdint>
#include <cstring>
#include "engine/core/shm_layout.h"

namespace egbos::l2 {

// ── Schema version ─────────────────────────────────────────────────────────
// Monotonically increasing. Increment whenever the layout changes.
// L3 must reject (or passively ignore) vectors whose schema_version it does
// not recognise. Version 0 means "skeleton only — no features are valid."
//
// PROVISIONAL LAYOUT — NOT EGBOS-APPROVED
inline constexpr uint8_t kFeatureVectorSchemaVersion = 0u;

// ── FeatureVectorLayout ─────────────────────────────────────────────────────
//
// Overlay on the 256-byte data[] array inside FeatureVectorRegion.
// All fields tagged // PROVISIONAL LAYOUT — NOT EGBOS-APPROVED
//
// Byte map (all values little-endian):
//   [  0 ]      schema_version      uint8_t
//   [  1 ]      reserved_flags      uint8_t     (must be 0)
//   [  2 –  7]  reserved_header     uint8_t[6]  (must be 0; pads to uint64_t alignment)
//   [  8 – 15]  validity_mask_lo64  uint64_t    (bitmask; bit N=1 ↔ feature N valid)
//   [ 16 –255]  feature_payload     uint8_t[240] (all 0 until a feature contract exists)
//
// Field ordering rationale: schema_version (1B) + reserved_flags (1B) +
// reserved_header[6] (6B) = 8B header → places uint64_t at natural 8-byte
// alignment, avoiding implicit compiler padding that would break sizeof==256.
//
// The validity_mask is the primary signal L3 uses to know which feature slots
// contain computed values. A zeroed mask means "no features are valid" and
// is the correct initial state.
struct FeatureVectorLayout {
    // PROVISIONAL LAYOUT — NOT EGBOS-APPROVED
    uint8_t  schema_version;       // byte 0
    uint8_t  reserved_flags;       // byte 1  — must remain 0
    uint8_t  reserved_header[6];   // bytes 2–7 — must remain 0 (alignment padding)
    uint64_t validity_mask_lo64;   // bytes 8–15: one bit per feature slot
    uint8_t  feature_payload[240]; // bytes 16–255 — all 0 until features approved
};

static_assert(sizeof(FeatureVectorLayout) == egbos::shm::kFeatureVectorBytes,
    "FeatureVectorLayout must be exactly kFeatureVectorBytes (256) bytes");
static_assert(offsetof(FeatureVectorLayout, schema_version)     ==   0, "layout check");
static_assert(offsetof(FeatureVectorLayout, reserved_flags)     ==   1, "layout check");
static_assert(offsetof(FeatureVectorLayout, reserved_header)    ==   2, "layout check");
static_assert(offsetof(FeatureVectorLayout, validity_mask_lo64) ==   8, "layout check");
static_assert(offsetof(FeatureVectorLayout, feature_payload)    ==  16, "layout check");


// ── build_skeleton_vector ──────────────────────────────────────────────────
//
// Constructs a zero-payload FeatureVector with schema_version and
// validity_mask set. The feature_payload is left all-zero (no features
// approved). Returns the raw 256-byte buffer ready to pass to
// FeatureVectorRegion::write().
//
// This is the only valid output of the FeatureEngine until EGBOS approves
// at least one feature contract.
inline void build_skeleton_vector(
    uint8_t (&out)[egbos::shm::kFeatureVectorBytes],
    uint64_t validity_mask = 0u) noexcept
{
    // PROVISIONAL LAYOUT — NOT EGBOS-APPROVED
    std::memset(out, 0, sizeof(out));
    auto* layout = reinterpret_cast<FeatureVectorLayout*>(out);
    layout->schema_version     = kFeatureVectorSchemaVersion;
    layout->reserved_flags     = 0u;
    layout->validity_mask_lo64 = validity_mask;
    // feature_payload: all zero — no approved features. See governance/claims/ for pending feature contracts (e.g. CLAIM-L2-HAWKES-001).
}

} // namespace egbos::l2
