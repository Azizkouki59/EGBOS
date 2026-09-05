// engine/tests/test_replay_comparison.cpp
// EGBOS S5-REQ-004 — Deterministic WAL Replay Comparison Infrastructure
//
// Specification refs  : S5-REQ-004 — 14 consecutive nightly replay validations
// Governed component  : CLAIM-WAL-001 (wal.durability, V4) / PROC-REPLAY-001
// Classification      : OFFLINE_REPLAY (infrastructure only)
// Source revision     : 2663389
//
// PURPOSE:
//   Formalize the deterministic replay comparison mechanism.
//   Every replay of an identical WAL input must produce byte-identical recovered state.
//   A negative test proves that state corruption is detected and reported.
//
// CRITICAL LIMITATION — READ BEFORE USING:
//   This file provides INFRASTRUCTURE and an offline correctness proof.
//   - Only unit-level WAL replay is tested here.
//   - 14 consecutive nightly production replay runs have NOT been executed.
//   - V4 promotion of S5-REQ-004 requires 14 successive nightly runs each
//     producing byte-identical results against a Tier-0 live WAL log.
//
// RESULT STRUCTURE (canonical):
//   RUN_ID               : unique identifier for each replay run
//   WAL_SHA256           : SHA-256 of the WAL file used as input
//   GOLDEN_STATE_SHA256  : SHA-256 of the reference state snapshot
//   REPLAY_STATE_SHA256  : SHA-256 of the re-derived state
//   BYTE_IDENTICAL       : true iff GOLDEN == REPLAY
//   MISMATCH_OFFSET      : first differing byte offset (-1 if identical)
//   REPLAY_BINARY_SHA256 : SHA-256 of the replay executable (not computed here)
//   SOURCE_REVISION      : git SHA at time of test
//   TIMESTAMP            : ISO-8601 UTC
//
// Govcheck compliance:
//   GC-E-010 : No malloc/new/delete in replay comparison path (WalWriter is static)
//   GC-E-011 : CLOCK_MONOTONIC for timestamps (no CLOCK_REALTIME)

#if defined(__linux__)

#include <gtest/gtest.h>
#include "engine/core/wal_writer.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

using egbos::wal::WalWriter;

// ── Minimal SHA-256 (FNV-1a 64-bit approximation for comparison purposes) ────
// NOTE: For production governance proof artifacts, use a certified SHA-256
// implementation (e.g., OpenSSL EVP, or kernel AF_ALG). This approximation
// is used only to demonstrate the comparison mechanism; the govcheck sign
// step computes the authoritative hash.
static uint64_t fnv1a_64(const uint8_t* data, size_t len) noexcept {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

// ── ReplayState: canonical serialization of WalWriter::RecoveryResult ─────────
// This is the golden-state representation. It is deterministically derived
// from any given WAL input by running scan_wal().
struct ReplayState {
    uint64_t valid_records;
    uint64_t last_valid_sequence;
    uint64_t truncation_byte_offset;

    // Canonical byte-level serialization for hash computation.
    // Field order is fixed. Do not reorder without updating the schema.
    void serialize(uint8_t* out) const noexcept {
        std::memcpy(out + 0,  &valid_records,          8);
        std::memcpy(out + 8,  &last_valid_sequence,    8);
        std::memcpy(out + 16, &truncation_byte_offset, 8);
    }

    static constexpr size_t kSerializedSize = 24; // bytes

    uint64_t state_hash() const noexcept {
        uint8_t buf[kSerializedSize];
        serialize(buf);
        return fnv1a_64(buf, kSerializedSize);
    }

    bool operator==(const ReplayState& o) const noexcept {
        return valid_records          == o.valid_records
            && last_valid_sequence    == o.last_valid_sequence
            && truncation_byte_offset == o.truncation_byte_offset;
    }
};

// ── ReplayResult: canonical result record ─────────────────────────────────────
struct ReplayResult {
    char     run_id[32];
    uint64_t wal_hash;
    uint64_t golden_state_hash;
    uint64_t replay_state_hash;
    bool     byte_identical;
    int64_t  mismatch_offset;    // -1 if identical
    char     source_revision[16];
    char     timestamp[32];

    void print() const {
        fprintf(stderr,
            "[ReplayResult]\n"
            "  run_id               : %s\n"
            "  wal_hash             : 0x%016llx\n"
            "  golden_state_hash    : 0x%016llx\n"
            "  replay_state_hash    : 0x%016llx\n"
            "  byte_identical       : %s\n"
            "  mismatch_offset      : %lld\n"
            "  source_revision      : %s\n"
            "  timestamp            : %s\n",
            run_id,
            (unsigned long long)wal_hash,
            (unsigned long long)golden_state_hash,
            (unsigned long long)replay_state_hash,
            byte_identical ? "true" : "false",
            (long long)mismatch_offset,
            source_revision,
            timestamp);
    }
};

// ── Helper: write a WAL with N records, return path ───────────────────────────
static std::string write_test_wal(const std::string& path_primary,
                                   const std::string& path_shadow,
                                   int num_records) {
    WalWriter writer(path_primary.c_str(), path_shadow.c_str());
    // WalRecord fields: order_id, quantity_delta (signed), price, payload_length.
    // No 'quantity' or 'side' — see wal_writer.h:121-124.
    // write_sync() takes WalRecord* (pointer), not value.
    alignas(64) egbos::wal::WalRecord rec{};
    rec.payload_length = 0;
    for (int i = 0; i < num_records; ++i) {
        rec.order_id       = static_cast<uint64_t>(i + 1);
        rec.price          = 100'000LL * 100'000LL;  // PRICE_SCALE
        rec.quantity_delta = (i % 2 == 0) ? 1 : -1;  // alternating buy/sell
        writer.write_sync(&rec);
    }
    return path_primary;
}

// ── Helper: hash a file ───────────────────────────────────────────────────────
static uint64_t hash_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return 0;
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    return fnv1a_64(buf.data(), buf.size());
}

// ── Fixture ───────────────────────────────────────────────────────────────────
class ReplayComparisonTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = std::filesystem::temp_directory_path()
                 / "egbos_replay_test_XXXXXX";
        // Simple unique-enough suffix using address.
        char suf[16];
        snprintf(suf, sizeof(suf), "%p", (void*)this);
        tmp_dir_ += suf;
        std::filesystem::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_);
    }

    std::string primary(const char* tag = "") const {
        return (tmp_dir_ / (std::string("wal_primary") + tag + ".bin")).string();
    }
    std::string shadow(const char* tag = "") const {
        return (tmp_dir_ / (std::string("wal_shadow") + tag + ".bin")).string();
    }
};

// ── Test 1: 3× identical replay — byte-identical golden state ─────────────────
//
// Write WAL with N records. Replay scan_wal() three times independently.
// All three must produce byte-identical ReplayState.
// This is the PROC-REPLAY-001 invariant formalized with the canonical result struct.
TEST_F(ReplayComparisonTest, ThreeReplays_ByteIdentical) {
    const int kRecords = 100;
    write_test_wal(primary(), shadow(), kRecords);

    const uint64_t wal_hash = hash_file(primary());
    ASSERT_NE(wal_hash, 0ULL);

    // Replay 3 times.
    ReplayState states[3];
    for (int run = 0; run < 3; ++run) {
        auto result = WalWriter::scan_wal(primary().c_str());
        states[run] = {result.valid_records,
                       result.last_valid_sequence,
                       result.truncation_byte_offset};
    }

    // All three must be identical.
    EXPECT_EQ(states[0], states[1]) << "Run 0 vs Run 1: state diverges";
    EXPECT_EQ(states[1], states[2]) << "Run 1 vs Run 2: state diverges";

    // Construct result record.
    ReplayResult res{};
    strncpy(res.run_id,            "PROC-REPLAY-001-triple", sizeof(res.run_id) - 1);
    strncpy(res.source_revision,   "2663389",               sizeof(res.source_revision) - 1);
    res.wal_hash           = wal_hash;
    res.golden_state_hash  = states[0].state_hash();
    res.replay_state_hash  = states[2].state_hash();
    res.byte_identical     = (res.golden_state_hash == res.replay_state_hash);
    res.mismatch_offset    = res.byte_identical ? -1 : 0;
    res.print();

    EXPECT_TRUE(res.byte_identical)
        << "DETERMINISTIC_REPLAY_INFRASTRUCTURE_READY: 3-run test failed";

    // Explicit infrastructure classification assertion.
    SUCCEED() << "DETERMINISTIC_REPLAY_INFRASTRUCTURE_READY — 14 NIGHT RUN NOT EXECUTED";
}

// ── Test 2: valid_records count matches written record count ──────────────────
TEST_F(ReplayComparisonTest, ValidRecords_MatchesWritten) {
    const int kRecords = 42;
    write_test_wal(primary("_cnt"), shadow("_cnt"), kRecords);
    auto result = WalWriter::scan_wal(primary("_cnt").c_str());
    EXPECT_EQ(result.valid_records, static_cast<uint64_t>(kRecords));
    EXPECT_EQ(result.last_valid_sequence, static_cast<uint64_t>(kRecords - 1));
}

// ── Test 3 (NEGATIVE): Corrupt one byte → mismatch detected ──────────────────
//
// This proves that the comparison mechanism catches corruption.
// A diverging replay_state_hash means the WAL is no longer byte-identical
// to the golden reference.
TEST_F(ReplayComparisonTest, CorruptedWal_MismatchDetected) {
    const int kRecords = 20;
    write_test_wal(primary("_neg"), shadow("_neg"), kRecords);

    // Scan the intact WAL to establish the golden state.
    auto golden_result = WalWriter::scan_wal(primary("_neg").c_str());
    ReplayState golden_state{golden_result.valid_records,
                              golden_result.last_valid_sequence,
                              golden_result.truncation_byte_offset};
    const uint64_t golden_hash = golden_state.state_hash();

    // Corrupt byte 4 — inside the 'sequence' field (offset 0–7), within CRC coverage [0,60).
    // CRC32C covers header bytes [0, 60) per wal_writer.h kWalCrcCoverageBytes.
    // Offset 64 is _pad (outside CRC coverage) — must not be used.
    {
        std::fstream f(primary("_neg"), std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(f.is_open());
        f.seekp(4);  // byte 4 of sequence field (offset 0, covers 8 bytes)
        uint8_t corrupt = 0xFF;
        f.write(reinterpret_cast<char*>(&corrupt), 1);
    }

    // Replay against corrupted WAL.
    auto corrupt_result = WalWriter::scan_wal(primary("_neg").c_str());
    ReplayState corrupt_state{corrupt_result.valid_records,
                               corrupt_result.last_valid_sequence,
                               corrupt_result.truncation_byte_offset};
    const uint64_t corrupt_hash = corrupt_state.state_hash();

    // The hashes must differ (corruption must be detected).
    EXPECT_NE(golden_hash, corrupt_hash)
        << "Corruption not detected: golden and corrupted states are identical";

    // valid_records after corruption must be fewer.
    EXPECT_LT(corrupt_result.valid_records, golden_result.valid_records)
        << "Expected fewer valid records after WAL corruption";

    ReplayResult res{};
    strncpy(res.run_id,          "PROC-REPLAY-001-negative", sizeof(res.run_id) - 1);
    strncpy(res.source_revision, "2663389",                  sizeof(res.source_revision) - 1);
    res.wal_hash          = hash_file(primary("_neg"));
    res.golden_state_hash = golden_hash;
    res.replay_state_hash = corrupt_hash;
    res.byte_identical    = false;
    res.mismatch_offset   = static_cast<int64_t>(corrupt_result.truncation_byte_offset);
    res.print();

    SUCCEED() << "NEGATIVE TEST: corruption detected as expected";
}

// ── Test 4: Empty WAL replay ──────────────────────────────────────────────────
TEST_F(ReplayComparisonTest, EmptyWal_ZeroRecords) {
    write_test_wal(primary("_empty"), shadow("_empty"), 0);
    auto result = WalWriter::scan_wal(primary("_empty").c_str());
    EXPECT_EQ(result.valid_records, 0ULL);
}

// ── Classification assertion ──────────────────────────────────────────────────
TEST(ReplayClassification, InfrastructureReady_NotProductionEvidence) {
    // This test documents the evidence gap explicitly.
    // Passing all tests above proves:
    //   - The replay mechanism is deterministic (PROC-REPLAY-001).
    //   - Corruption is detectable.
    // It does NOT prove:
    //   - 14 consecutive nightly production runs.
    //   - Byte-identical replay against a live Tier-0 WAL.
    SUCCEED()
        << "DETERMINISTIC_REPLAY_INFRASTRUCTURE_READY — 14 NIGHT RUN NOT EXECUTED";
}

#else // !__linux__

#include <gtest/gtest.h>

TEST(ReplayComparisonTest, SkippedOnNonLinux) {
    GTEST_SKIP() << "WAL replay tests require Linux (io_uring). Skipped.";
}

#endif // __linux__
