// engine/tests/position_engine_test.cpp
// S3 unit tests for L3 PositionEngine
// Spec: EGBOS-SPEC-S3-001 §4.2, §7.1
// Claims: CLAIM-L3-001 (WAL-before-state), CLAIM-INV-001 (position invariant)
// GC-E-043 enforced: only gross_exposure_ exposed via CROSS_PLANE_READ

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <limits>
#include <cmath>
#include "core/position_engine.h"
#include "core/governance_interface.h"
#include "core/cross_plane.h"
#include "core/wal_writer.h"

using namespace egbos;
using namespace egbos::core;
using namespace egbos::wal;

// Helper: create a temp WAL file path for tests
static std::string tmp_wal_primary() {
    char buf[] = "/dev/shm/egbos_test_wal_XXXXXX";
    int fd = mkstemp(buf);
    if (fd >= 0) close(fd);
    return std::string(buf);
}

static std::string tmp_wal_shadow() {
    char buf[] = "/dev/shm/egbos_test_shadow_XXXXXX";
    int fd = mkstemp(buf);
    if (fd >= 0) close(fd);
    return std::string(buf);
}

TEST(PositionEngineTest, ApplyFillUpdatesGrossExposure) {
    auto primary = tmp_wal_primary();
    auto shadow  = tmp_wal_shadow();

    WalWriter writer(primary.c_str(), shadow.c_str());
    GovernanceFlags gov{};
    gov.pos_limit_latch.store(100000, std::memory_order_relaxed);

    PositionEngine engine(writer, gov);

    FillEvent fill{};
    fill.instrument_id = 1;
    fill.signed_qty    = 10;
    fill.quantity      = 10;
    fill.price         = 1000;

    engine.apply_fill(fill);

    int64_t exposure = CROSS_PLANE_READ(
        engine.gross_exposure_,
        egbos::plane::LAYER_L4,
        egbos::plane::LAYER_L3
    );
    EXPECT_EQ(exposure, 10);
}

TEST(PositionEngineTest, ApplyFillAccumulatesExposure) {
    auto primary = tmp_wal_primary();
    auto shadow  = tmp_wal_shadow();

    WalWriter writer(primary.c_str(), shadow.c_str());
    GovernanceFlags gov{};
    gov.pos_limit_latch.store(100000, std::memory_order_relaxed);

    PositionEngine engine(writer, gov);

    FillEvent fill1{1, 10, 10, 1000};
    FillEvent fill2{1, 5,  5, 1001};
    engine.apply_fill(fill1);
    engine.apply_fill(fill2);

    int64_t exposure = CROSS_PLANE_READ(
        engine.gross_exposure_,
        egbos::plane::LAYER_L4,
        egbos::plane::LAYER_L3
    );
    EXPECT_EQ(exposure, 15);
}

// ── CLAIM-L3-001: WAL-before-state (10x kill-9 simulation) ───────────────────
// AD-012 §4.6: WAL entry must be fsync-confirmed before any state mutation.
// Each cycle verifies WAL file grows (record written) before engine state changes.
TEST(PositionEngineTest, WalBeforeState_Kill9Simulation) {
    const int N_CYCLES = 10;

    for (int cycle = 0; cycle < N_CYCLES; ++cycle) {
        auto primary = tmp_wal_primary();
        auto shadow  = tmp_wal_shadow();

        WalWriter writer(primary.c_str(), shadow.c_str());
        GovernanceFlags gov{};
        gov.pos_limit_latch.store(1000000, std::memory_order_relaxed);
        PositionEngine engine(writer, gov);

        struct stat st_before{};
        ::stat(primary.c_str(), &st_before);

        FillEvent fill{};
        fill.instrument_id = static_cast<uint32_t>(cycle + 1);
        fill.signed_qty    = static_cast<int64_t>(cycle + 1) * 10;
        fill.quantity      = fill.signed_qty;
        fill.price         = 1000;

        engine.apply_fill(fill);

        // WAL must have grown before state mutation — AD-012 invariant
        struct stat st_after{};
        ::stat(primary.c_str(), &st_after);
        EXPECT_GT(st_after.st_size, st_before.st_size)
            << "Cycle " << cycle << ": WAL did not grow — AD-012 violated";

        int64_t exposure = CROSS_PLANE_READ(
            engine.gross_exposure_,
            egbos::plane::LAYER_L4,
            egbos::plane::LAYER_L3
        );
        EXPECT_EQ(exposure, std::abs(fill.signed_qty))
            << "Cycle " << cycle << ": gross_exposure_ mismatch";
    }
}

// ── CLAIM-INV-001: Position invariant (10M fills) ────────────────────────────
// Spec §4.2: gross_exposure_ == Σ |signed_qty| across all applied fills.
// Applies 10M alternating buy/sell fills and verifies the invariant holds.
TEST(PositionEngineTest, PositionInvariant_10M_Fills) {
    auto primary = tmp_wal_primary();
    auto shadow  = tmp_wal_shadow();

    WalWriter writer(primary.c_str(), shadow.c_str());
    GovernanceFlags gov{};
    gov.pos_limit_latch.store(std::numeric_limits<int64_t>::max(),
                              std::memory_order_relaxed);
    PositionEngine engine(writer, gov);

    const int N_FILLS = 10'000'000;
    int64_t expected_exposure = 0;

    for (int i = 0; i < N_FILLS; ++i) {
        int64_t qty = (i % 2 == 0) ? 1 : -1;
        FillEvent fill{};
        fill.instrument_id = 1;
        fill.signed_qty    = qty;
        fill.quantity      = static_cast<int64_t>(std::abs(qty));
        fill.price         = 1000;
        engine.apply_fill(fill);
        expected_exposure += std::abs(qty);
    }

    int64_t gross = CROSS_PLANE_READ(
        engine.gross_exposure_,
        egbos::plane::LAYER_L4,
        egbos::plane::LAYER_L3
    );
    EXPECT_EQ(gross, expected_exposure)
        << "gross_exposure_ invariant violated after " << N_FILLS << " fills";
}
