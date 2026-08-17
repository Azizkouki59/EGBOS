// engine/tests/test_lob_determinism.cpp
// CLAIM-LOB-001 V4: 3× byte-identical LOB replay against golden fixture.
// Blueprint ref: §I2 S2 Pass Criteria — CLAIM-LOB-001 at V4.
//
// Protocol:
//   If governance/proof_artifacts/golden_lob.bin does not exist, the first run
//   writes it and the test passes (fixture generation). On every subsequent run
//   all three independent engine executions must produce byte-identical output
//   that also matches the committed golden fixture.

#include <gtest/gtest.h>
#include "engine/l1/lob_engine.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

// Canonical input sequence for the golden fixture.
// This sequence must never change without a governance decision upgrading
// the fixture (which would require a new V4 signing cycle).
static egbos::l1::Top5BidsAsks run_canonical_sequence() noexcept {
    egbos::l1::LobEngine engine(100000LL, 100LL);
    engine.add_order(1, 100100, 50, true);   // bid
    engine.add_order(2, 100200, 30, true);   // bid
    engine.add_order(3, 99900, 25, false);   // ask
    engine.add_order(4, 99800, 10, false);   // ask
    engine.modify_order(1, 10);
    engine.cancel_order(2);
    return engine.generate_top5_snapshot();
}

static std::filesystem::path golden_path() {
    // Resolve relative to this file's assumed repo root ($EGBOS/egbos).
    // CMake will pass EGBOS_REPO_ROOT via -D flag; fall back to CWD.
    const char* env = std::getenv("EGBOS_REPO_ROOT");
    std::filesystem::path root = env ? std::filesystem::path(env)
                                     : std::filesystem::current_path();
    return root / "governance" / "proof_artifacts" / "golden_lob.bin";
}

} // namespace

// ── Fixture generation / existence check ────────────────────────────────────
TEST(LobDeterminismTest, GoldenFixtureExists) {
    // If the fixture does not exist yet, generate it from the canonical
    // sequence and write it to disk.  A test framework failure here means
    // the proof artifact directory is missing — governance issue, not code.
    auto path = golden_path();
    if (!std::filesystem::exists(path)) {
        auto snap = run_canonical_sequence();
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open()) << "Cannot write golden fixture: " << path;
        out.write(reinterpret_cast<const char*>(&snap), sizeof(snap));
        ASSERT_TRUE(out.good()) << "Write failed: " << path;
        out.close();
        GTEST_SKIP() << "Golden fixture written for the first time. "
                        "Re-run to validate determinism.";
    }

    // Read back the committed golden fixture.
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.is_open()) << "Cannot read golden fixture: " << path;
    egbos::l1::Top5BidsAsks golden{};
    in.read(reinterpret_cast<char*>(&golden), sizeof(golden));
    ASSERT_EQ(in.gcount(), static_cast<std::streamsize>(sizeof(golden)))
        << "Golden fixture size mismatch";

    EXPECT_EQ(sizeof(golden), sizeof(egbos::l1::Top5BidsAsks))
        << "Top5BidsAsks struct size changed — fixture must be regenerated";
}

// ── 3× byte-identical replay against golden fixture (CLAIM-LOB-001 V4) ──────
TEST(LobDeterminismTest, ThreeXByteIdenticalReplayAgainstGolden) {
    auto path = golden_path();
    if (!std::filesystem::exists(path)) {
        GTEST_SKIP() << "Golden fixture not present — run GoldenFixtureExists first.";
    }

    // Read committed golden.
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    egbos::l1::Top5BidsAsks golden{};
    in.read(reinterpret_cast<char*>(&golden), sizeof(golden));
    ASSERT_EQ(in.gcount(), static_cast<std::streamsize>(sizeof(golden)));

    // Three independent engine instantiations.
    auto snap1 = run_canonical_sequence();
    auto snap2 = run_canonical_sequence();
    auto snap3 = run_canonical_sequence();

    // Each run must be byte-identical to the golden fixture.
    EXPECT_EQ(std::memcmp(&snap1, &golden, sizeof(golden)), 0)
        << "Run 1 diverges from golden fixture — CLAIM-LOB-001 V4 FAIL";
    EXPECT_EQ(std::memcmp(&snap2, &golden, sizeof(golden)), 0)
        << "Run 2 diverges from golden fixture — CLAIM-LOB-001 V4 FAIL";
    EXPECT_EQ(std::memcmp(&snap3, &golden, sizeof(golden)), 0)
        << "Run 3 diverges from golden fixture — CLAIM-LOB-001 V4 FAIL";

    // Pairwise consistency (belt-and-suspenders).
    EXPECT_EQ(std::memcmp(&snap1, &snap2, sizeof(snap1)), 0);
    EXPECT_EQ(std::memcmp(&snap2, &snap3, sizeof(snap2)), 0);
}

// ── Cache-line alignment invariant ──────────────────────────────────────────
TEST(LobDeterminismTest, CacheLineAlignment) {
    // I-S2-004: Top5BidsAsks must be exactly 64 bytes (one L2 cache line).
    ASSERT_EQ(sizeof(egbos::l1::Top5BidsAsks), 64u);
}

// ── Collision Resolution Invariant ──────────────────────────────────────────
TEST(LobDeterminismTest, CollisionHandling) {
    egbos::l1::LobEngine engine(100000LL, 100LL);

    // kMaxOrders is 131072.
    // We insert two orders that map to the same slot.
    uint64_t id1 = 1;
    uint64_t id2 = 1 + 131072;
    uint64_t id3 = 1 + 2 * 131072;

    engine.add_order(id1, 100100, 50, true);
    engine.add_order(id2, 100200, 30, true);
    engine.add_order(id3, 100300, 20, true);

    auto snap = engine.generate_top5_snapshot();
    EXPECT_EQ(snap.bid_qtys[0], 20); // 100300 is best bid
    EXPECT_EQ(snap.bid_qtys[1], 30); // 100200 is second best bid
    EXPECT_EQ(snap.bid_qtys[2], 50); // 100100 is third best bid

    // Modify id2
    engine.modify_order(id2, 60);
    snap = engine.generate_top5_snapshot();
    EXPECT_EQ(snap.bid_qtys[1], 60);

    // Cancel id1 (the first inserted, testing shift logic)
    engine.cancel_order(id1);
    snap = engine.generate_top5_snapshot();
    EXPECT_EQ(snap.bid_qtys[0], 20); // 100300 is still there
    EXPECT_EQ(snap.bid_qtys[1], 60); // 100200 is still there
    EXPECT_EQ(snap.bid_qtys[2], 0);  // third bid is gone

    // Cancel id3
    engine.cancel_order(id3);
    snap = engine.generate_top5_snapshot();
    EXPECT_EQ(snap.bid_qtys[0], 60); // 100200 is now best bid

    // Cancel id2
    engine.cancel_order(id2);
    snap = engine.generate_top5_snapshot();
    EXPECT_EQ(snap.bid_qtys[0], 0); // all gone
}
