// tests/governance/fixtures/violation_GC_E_045.cpp
// §15.4 Violation Fixture: GC-E-045 (direct x_l_/order_tag_ read from L4/L5)
//
// This file intentionally reads x_l_ and order_tag_ directly from
// L4 (Signal/Model) or L5 (Research) code instead of using the
// CROSS_PLANE_READ macro. govcheck source scanning must detect this
// and fire GC-E-045.
//
// Pre-registered for Phase 4 — the rule must exist and fire even though
// the full layer isolation audit is Phase 4 scope.
//
// Expected: govcheck reports GC-E-045 on this file.

#include <atomic>
#include <cstdint>

namespace egbos::signal {

// Simulated cross-plane state (should only be read via CROSS_PLANE_READ)
struct CrossPlaneView {
    std::atomic<int64_t> x_l_{0};
    std::atomic<int64_t> order_tag_{0};
};

// VIOLATION: Direct read of x_l_ from L4 code without CROSS_PLANE_READ
// L4/L5 code must use CROSS_PLANE_READ(field) macro (§6.3, §14.4).
int64_t direct_read_VIOLATION(const CrossPlaneView& view) {
    // GC-E-045: direct x_l_ read from L4/L5
    int64_t val = view.x_l_.load(std::memory_order_acquire);
    // GC-E-045: direct order_tag_ read from L4/L5
    val += view.order_tag_.load(std::memory_order_acquire);
    return val;
}

}  // namespace egbos::signal
