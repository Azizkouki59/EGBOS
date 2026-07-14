// tests/governance/fixtures/violation_GC_E_011.cpp
// §15.4 Violation Fixture: GC-E-011 (CLOCK_REALTIME in engine/)
//
// This file intentionally contains a CLOCK_REALTIME call inside engine/ code.
// govcheck source scanning must detect this and fire GC-E-011.
// If this fixture passes CI without triggering GC-E-011, the rule is broken.
//
// Pre-registered for Phase 2 — the rule must exist and fire even though
// the full realtime-clock audit is Phase 2 scope.
//
// Expected: govcheck reports GC-E-011 on this file.

#include <cstdint>
#include <time.h>

namespace egbos::core {

// VIOLATION: CLOCK_REALTIME in engine/ — forbidden by GC-E-011
uint64_t get_wall_clock_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);  // GC-E-011: CLOCK_REALTIME in engine/
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
}

}  // namespace egbos::core
