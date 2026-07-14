// tests/governance/fixtures/violation_GC_WAL_001.cpp
// §15.4 Violation Fixture: GC-WAL-001 (state mutation before WAL fsync)
//
// This file intentionally mutates shared state before the WAL write
// has completed (O_DSYNC return). govcheck source scanning must detect
// this pattern and fire GC-WAL-001.
// If this fixture passes CI without triggering GC-WAL-001, the rule is broken.
//
// Expected: govcheck reports GC-WAL-001 (Blocking) on this file.

#include <atomic>
#include <cstdint>

namespace egbos::core {

// Simulated position state — shared memory
struct PositionState {
    std::atomic<int64_t> net_quantity{0};
};

// Simulated WAL write function (not yet returned from O_DSYNC)
void wal_write_pending();

// VIOLATION: Mutating position state BEFORE WAL fsync completes.
// The three-phase commit protocol requires:
//   Phase 1: Prepare — validate, compute
//   Phase 2: Write/Fsync — WAL write_sync() blocks until O_DSYNC
//   Phase 3: Apply — mutate position state AFTER Phase 2 returns
//
// This code mutates state in Phase 1, before wal_write_pending().
void apply_fill_VIOLATION(PositionState& pos, int64_t delta) {
    // GC-WAL-001: state mutation before WAL fsync
    pos.net_quantity.store(
        pos.net_quantity.load(std::memory_order_relaxed) + delta,
        std::memory_order_release);

    // WAL write happens AFTER state mutation — WRONG ORDER
    wal_write_pending();
}

}  // namespace egbos::core
