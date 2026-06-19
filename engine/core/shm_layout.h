// engine/core/shm_layout.h
// AD-006: SHM Region Sizes as Compile-Time Constants
// All SHM region sizes are constexpr. No runtime configuration.
// Changes require recompile and re-measurement of affected timing constants.
//
// Populated with real values in S1 (WAL + SPSC + SHM Skeleton).

#pragma once

#include <cstddef>
#include <cstdint>

namespace egbos::core {

// Placeholder constants — populated in S1
constexpr std::size_t kMaxInstruments = 256;
constexpr std::size_t kShmRegionAlignment = 4096;  // Page-aligned

// SHM region sizes will be derived from kMaxInstruments
// and timing_constants.json in S1.

}  // namespace egbos::core
