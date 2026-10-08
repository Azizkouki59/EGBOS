// engine/l2/feature_abi.h
// EGBOS — Neutral header for shared L2 Feature ABI definitions.
//
// This isolates schema versions and constants to prevent circular dependencies
// or inappropriate coupling between reader and publisher headers.

#pragma once

#include <cstdint>

namespace egbos::l2 {

// Current production schema version.  L3 must reject any version it does not
// know.  Increment whenever the FeatureSlot ABI changes incompatibly.
inline constexpr uint8_t kFeatureSlotSchemaVersion = 1u;

} // namespace egbos::l2
