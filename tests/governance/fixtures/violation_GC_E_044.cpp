// tests/governance/fixtures/violation_GC_E_044.cpp
// §15.4 Violation Fixture: GC-E-044 (memory_order_relaxed on cross-plane atomic)
//
// This file intentionally uses memory_order_relaxed when reading a
// cross-plane atomic (x_l_ or order_tag_). govcheck source scanning
// must detect this and fire GC-E-044.
//
// Pre-registered for Phase 4 — the rule must exist and fire even though
// the full cross-plane audit is Phase 4 scope.
//
// Expected: govcheck reports GC-E-044 on this file.

#include <atomic>
#include <cstdint>

namespace egbos::core {

struct CrossPlaneState {
    std::atomic<int64_t> x_l_{0};
};

// VIOLATION: memory_order_relaxed on cross-plane atomic read
// Cross-plane reads must use memory_order_acquire (§6.3, §14.4).
int64_t read_cross_plane_VIOLATION(const CrossPlaneState& state) {
    // GC-E-044: memory_order_relaxed on cross-plane atomic
    return state.x_l_.load(std::memory_order_relaxed);
}

}  // namespace egbos::core
