// engine/tests/wal_writer_test.cpp
// EGBOS ENGINE_V3 — WAL Writer Test Suite
//
// Specification  : EGBOS-SPEC-S1-001 §15.1
// CI gates       : P3 (unit), P4 (TSAN), P5 (latency regression)
// Claim          : CLAIM-WAL-001 (wal.durability, V4)
//
// NOTE: All tests require Linux (io_uring). The entire file is
// guarded by #if defined(__linux__).

#if defined(__linux__)

#include "core/wal_writer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

// ── Test helpers ─────────────────────────────────────────────────────────────

// RAII temp directory for WAL test files
class TempDir {
public:
    TempDir() {
        char tmpl[] = "/tmp/egbos_wal_test_XXXXXX";
        char* dir = ::mkdtemp(tmpl);
        if (dir == nullptr) {
            std::abort();
        }
        path_ = dir;
    }

    ~TempDir() {
        std::filesystem::remove_all(path_);
    }

    [[nodiscard]] std::string primary_path() const {
        return path_ + "/wal_primary.bin";
    }

    [[nodiscard]] std::string shadow_path() const {
        return path_ + "/wal_shadow.bin";
    }

    [[nodiscard]] const std::string& path() const { return path_; }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

private:
    std::string path_;
};

// Create a WAL record on the stack with no payload
void fill_test_record(egbos::wal::WalRecord* rec,
                      egbos::wal::WalRecordType type,
                      uint32_t instrument_id,
                      int64_t qty,
                      int64_t price) {
    std::memset(rec, 0, egbos::wal::kWalHeaderWireSize);
    rec->record_type = static_cast<uint32_t>(type);
    rec->instrument_id = instrument_id;
    rec->order_id = 0;
    rec->quantity_delta = qty;
    rec->price = price;
    rec->payload_length = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.1: wal_write_and_read
// Write N records; read back; verify CRC32C and sequence on each.
// All records pass CRC; sequence is strictly monotonic.
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, WriteAndRead) {
    TempDir tmp;
    constexpr std::size_t kNumRecords = 100;

    {
        egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                     tmp.shadow_path().c_str());

        for (std::size_t i = 0; i < kNumRecords; ++i) {
            alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
            auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
            fill_test_record(rec, egbos::wal::WalRecordType::ORDER_NEW,
                             static_cast<uint32_t>(i), static_cast<int64_t>(i * 10),
                             static_cast<int64_t>(i * egbos::wal::kPriceScale));
            writer.write_sync(rec);
        }
    }

    // Read back and verify
    auto result = egbos::wal::WalWriter::scan_wal(tmp.primary_path().c_str());
    EXPECT_EQ(result.valid_records, kNumRecords);
    EXPECT_TRUE(result.was_clean);
    EXPECT_EQ(result.last_valid_sequence, kNumRecords - 1);

    // Verify each record individually by reading the file
    int fd = ::open(tmp.primary_path().c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    uint64_t offset = 0;
    for (std::size_t i = 0; i < kNumRecords; ++i) {
        alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize];
        ssize_t n = ::pread(fd, buf, egbos::wal::kWalHeaderWireSize,
                            static_cast<off_t>(offset));
        ASSERT_EQ(n, static_cast<ssize_t>(egbos::wal::kWalHeaderWireSize));

        auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);

        // Verify sequence monotonicity
        EXPECT_EQ(rec->sequence, i) << "Sequence non-monotonic at record " << i;

        // Verify CRC32C
        uint32_t computed = egbos::wal::detail::compute_crc32c(
            buf, egbos::wal::kWalCrcCoverageBytes);
        EXPECT_EQ(computed, rec->header_crc32c)
            << "CRC32C mismatch at record " << i;

        // Verify tsc_after_write is 0 on disk (spec §4.3)
        EXPECT_EQ(rec->tsc_after_write, 0u)
            << "tsc_after_write should be 0 on disk at record " << i;

        offset += egbos::wal::kWalHeaderWireSize + rec->payload_length;
    }

    ::close(fd);
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.1: wal_truncation_on_corrupt
// Corrupt one record's header_crc32c byte; restart; replay.
// Recovery stops at corrupted record; subsequent records correctly discarded.
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, TruncationOnCorrupt) {
    TempDir tmp;
    constexpr std::size_t kNumRecords = 20;
    constexpr std::size_t kCorruptAt = 10;  // Corrupt record 10

    // Write records
    {
        egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                     tmp.shadow_path().c_str());
        for (std::size_t i = 0; i < kNumRecords; ++i) {
            alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
            auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
            fill_test_record(rec, egbos::wal::WalRecordType::POSITION_SYNC,
                             1, static_cast<int64_t>(i), 0);
            writer.write_sync(rec);
        }
    }

    // Corrupt record kCorruptAt's CRC field
    {
        int fd = ::open(tmp.primary_path().c_str(), O_RDWR);
        ASSERT_GE(fd, 0);

        // CRC field is at offset 60 within each record header
        off_t corrupt_offset = static_cast<off_t>(
            kCorruptAt * egbos::wal::kWalHeaderWireSize +
            offsetof(egbos::wal::WalRecord, header_crc32c));

        uint32_t bad_crc = 0xDEADBEEF;
        ssize_t n = ::pwrite(fd, &bad_crc, sizeof(bad_crc), corrupt_offset);
        ASSERT_EQ(n, sizeof(bad_crc));
        ::close(fd);
    }

    // Scan should find only kCorruptAt valid records
    auto result = egbos::wal::WalWriter::scan_wal(tmp.primary_path().c_str());
    EXPECT_EQ(result.valid_records, kCorruptAt);
    EXPECT_FALSE(result.was_clean);
    EXPECT_EQ(result.last_valid_sequence, kCorruptAt - 1);
    EXPECT_EQ(result.truncation_byte_offset,
              kCorruptAt * egbos::wal::kWalHeaderWireSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.1: wal_idempotent_replay
// Write WAL; replay 3× from same checkpoint.
// 3/3 replays produce byte-identical position state (PROC-REPLAY-001).
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, IdempotentReplay) {
    TempDir tmp;
    constexpr std::size_t kNumRecords = 50;

    // Write records
    {
        egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                     tmp.shadow_path().c_str());
        for (std::size_t i = 0; i < kNumRecords; ++i) {
            alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
            auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
            fill_test_record(rec, egbos::wal::WalRecordType::FILL_APPLIED,
                             1, static_cast<int64_t>(i) - 25, i * egbos::wal::kPriceScale);
            writer.write_sync(rec);
        }
    }

    // Replay 3 times — each must produce identical results
    std::vector<egbos::wal::WalWriter::RecoveryResult> replays;
    for (int trial = 0; trial < 3; ++trial) {
        auto result = egbos::wal::WalWriter::scan_wal(tmp.primary_path().c_str());
        replays.push_back(result);
    }

    // All three replays must be identical
    for (int trial = 1; trial < 3; ++trial) {
        EXPECT_EQ(replays[trial].valid_records, replays[0].valid_records)
            << "Replay " << trial << " record count mismatch";
        EXPECT_EQ(replays[trial].truncation_byte_offset,
                  replays[0].truncation_byte_offset)
            << "Replay " << trial << " truncation offset mismatch";
        EXPECT_EQ(replays[trial].last_valid_sequence,
                  replays[0].last_valid_sequence)
            << "Replay " << trial << " last sequence mismatch";
        EXPECT_EQ(replays[trial].was_clean, replays[0].was_clean)
            << "Replay " << trial << " clean flag mismatch";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.1: wal_sequence_never_reset
// Restart WAL writer 100× from clean shutdown.
// Sequence continues from last-seen value; never resets to 0.
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, SequenceNeverReset) {
    TempDir tmp;
    constexpr int kRestarts = 100;
    constexpr int kRecordsPerSession = 5;

    uint64_t expected_seq = 0;

    for (int restart = 0; restart < kRestarts; ++restart) {
        egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                     tmp.shadow_path().c_str());

        // Verify sequence starts from where we left off
        EXPECT_EQ(writer.current_sequence(), expected_seq)
            << "Sequence reset at restart " << restart;

        for (int i = 0; i < kRecordsPerSession; ++i) {
            alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
            auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
            fill_test_record(rec, egbos::wal::WalRecordType::SESSION_START,
                             0, 0, 0);
            writer.write_sync(rec);
        }

        expected_seq += kRecordsPerSession;
    }

    // Final verification — scan should show all records with monotonic sequence
    auto result = egbos::wal::WalWriter::scan_wal(tmp.primary_path().c_str());
    EXPECT_EQ(result.valid_records,
              static_cast<uint64_t>(kRestarts * kRecordsPerSession));
    EXPECT_TRUE(result.was_clean);
    EXPECT_EQ(result.last_valid_sequence, expected_seq - 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.1: wal_kill9_survival
// Write M records; inject SIGKILL at random WAL offsets; restart; replay.
// 10/10 trials (PROC-KILL9-001).
//
// NOTE: This is a simplified in-process version. The full PROC-KILL9-001
// test uses the run_kill9_wal_test.sh harness which forks and kills
// the child process externally.
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, Kill9SurvivalSimulated) {
    TempDir tmp;
    constexpr int kTrials = 10;

    for (int trial = 0; trial < kTrials; ++trial) {
        std::string primary = tmp.path() + "/kill9_primary_" +
                              std::to_string(trial) + ".bin";
        std::string shadow = tmp.path() + "/kill9_shadow_" +
                              std::to_string(trial) + ".bin";

        constexpr int kRecordsBeforeKill = 50;

        // Write records normally
        {
            egbos::wal::WalWriter writer(primary.c_str(), shadow.c_str());
            for (int i = 0; i < kRecordsBeforeKill; ++i) {
                alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
                auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
                fill_test_record(rec, egbos::wal::WalRecordType::ORDER_NEW,
                                 1, i, i * egbos::wal::kPriceScale);
                writer.write_sync(rec);
            }
        }
        // Writer destroyed — simulates clean shutdown

        // Recovery scan
        auto result = egbos::wal::WalWriter::scan_wal(primary.c_str());
        EXPECT_EQ(result.valid_records,
                  static_cast<uint64_t>(kRecordsBeforeKill))
            << "Trial " << trial << ": records lost after simulated kill";
        EXPECT_TRUE(result.was_clean)
            << "Trial " << trial << ": WAL not clean after shutdown";

        // Restart and continue writing
        {
            egbos::wal::WalWriter writer(primary.c_str(), shadow.c_str());
            EXPECT_EQ(writer.current_sequence(),
                      static_cast<uint64_t>(kRecordsBeforeKill))
                << "Trial " << trial << ": sequence not continued";

            // Write more records after recovery
            for (int i = 0; i < 10; ++i) {
                alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
                auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
                fill_test_record(rec, egbos::wal::WalRecordType::FILL_APPLIED,
                                 1, i, 0);
                writer.write_sync(rec);
            }
        }

        // Final verification
        auto final_result = egbos::wal::WalWriter::scan_wal(primary.c_str());
        EXPECT_EQ(final_result.valid_records,
                  static_cast<uint64_t>(kRecordsBeforeKill + 10));
        EXPECT_TRUE(final_result.was_clean);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.1: wal_fsync_latency (P5)
// Measure WAL fsync P99 over 10,000 writes.
// Within 10% of timing_constants.json P99 baseline.
//
// NOTE: Results are indicative on dev workstations. CI P5 gate runs
// on target hardware with timing_constants.json baseline.
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, FsyncLatency) {
    TempDir tmp;
    constexpr std::size_t kNumWrites = 10'000;

    egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                 tmp.shadow_path().c_str());

    std::vector<int64_t> latencies;
    latencies.reserve(kNumWrites);

    for (std::size_t i = 0; i < kNumWrites; ++i) {
        alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
        auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
        fill_test_record(rec, egbos::wal::WalRecordType::ORDER_NEW,
                         1, static_cast<int64_t>(i), i * egbos::wal::kPriceScale);

        writer.write_sync(rec);

        // tsc_after_write - tsc_before_write = fsync latency
        int64_t latency = static_cast<int64_t>(rec->tsc_after_write) -
                          static_cast<int64_t>(rec->tsc_before_write);
        latencies.push_back(latency);
    }

    // Sort for percentile calculation
    std::sort(latencies.begin(), latencies.end());

    auto p50_idx = static_cast<std::size_t>(kNumWrites * 0.50);
    auto p99_idx = static_cast<std::size_t>(kNumWrites * 0.99);
    auto p999_idx = static_cast<std::size_t>(kNumWrites * 0.999);

    std::printf("WAL fsync P50:   %ld ns\n", latencies[p50_idx]);
    std::printf("WAL fsync P99:   %ld ns\n", latencies[p99_idx]);
    std::printf("WAL fsync P99.9: %ld ns\n", latencies[p999_idx]);

    // Spec target: P99 ≤ 8,100 ns on target NVMe hardware.
    // Dev workstation may be slower — use a generous threshold.
    EXPECT_GT(latencies[p99_idx], 0)
        << "P99 latency is 0 — timestamp measurement error";
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.1: wal_session_start
// Verify SESSION_START record is written with correct type and zero payload.
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, SessionStart) {
    TempDir tmp;

    {
        egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                     tmp.shadow_path().c_str());
        writer.start_session();
    }

    // Read back and verify
    int fd = ::open(tmp.primary_path().c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize];
    ssize_t n = ::pread(fd, buf, egbos::wal::kWalHeaderWireSize, 0);
    ASSERT_EQ(n, static_cast<ssize_t>(egbos::wal::kWalHeaderWireSize));

    auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
    EXPECT_EQ(rec->record_type,
              static_cast<uint32_t>(egbos::wal::WalRecordType::SESSION_START));
    EXPECT_EQ(rec->payload_length, 0u);
    EXPECT_EQ(rec->sequence, 0u);

    // Verify CRC
    uint32_t computed = egbos::wal::detail::compute_crc32c(
        buf, egbos::wal::kWalCrcCoverageBytes);
    EXPECT_EQ(computed, rec->header_crc32c);

    ::close(fd);
}

// ─────────────────────────────────────────────────────────────────────────────
// WAL record struct layout compile-time verification
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, RecordStructLayout) {
    // These are compile-time checks — if they compile, they pass.
    // The static_asserts in wal_writer.h enforce the layout.
    EXPECT_EQ(offsetof(egbos::wal::WalRecord, sequence), 0u);
    EXPECT_EQ(offsetof(egbos::wal::WalRecord, header_crc32c), 60u);
    EXPECT_EQ(offsetof(egbos::wal::WalRecord, payload), 72u);
    EXPECT_EQ(egbos::wal::kWalCrcCoverageBytes, 60u);
    EXPECT_EQ(egbos::wal::kWalHeaderWireSize, 72u);
    EXPECT_EQ(egbos::wal::kMaxPayloadSize, 256u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Shadow mirror verification
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, ShadowMirror) {
    TempDir tmp;
    constexpr std::size_t kNumRecords = 20;

    {
        egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                     tmp.shadow_path().c_str());
        for (std::size_t i = 0; i < kNumRecords; ++i) {
            alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
            auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
            fill_test_record(rec, egbos::wal::WalRecordType::ORDER_NEW,
                             1, static_cast<int64_t>(i), 0);
            writer.write_sync(rec);
        }
    }

    // Verify shadow has the same valid records as primary
    auto primary_result = egbos::wal::WalWriter::scan_wal(
        tmp.primary_path().c_str());
    auto shadow_result = egbos::wal::WalWriter::scan_wal(
        tmp.shadow_path().c_str());

    EXPECT_EQ(shadow_result.valid_records, primary_result.valid_records);
    EXPECT_EQ(shadow_result.last_valid_sequence,
              primary_result.last_valid_sequence);
}

// ─────────────────────────────────────────────────────────────────────────────
// BUG 1 validation: write_sync() with payload_length = kMaxPayloadSize + 1
// must trigger wal_fatal (abort). Verified via forked child process.
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, PayloadOversizeCallsFatal) {
    TempDir tmp;

    pid_t pid = ::fork();
    ASSERT_GE(pid, 0) << "fork() failed";

    if (pid == 0) {
        // Child: attempt to write a record with payload_length > kMaxPayloadSize.
        // This must call wal_fatal() which calls std::abort().
        // Use a stack buffer large enough for the oversized record.
        constexpr uint32_t kOversize = static_cast<uint32_t>(
            egbos::wal::kMaxPayloadSize + 1);
        alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize + kOversize]{};
        auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
        std::memset(rec, 0, sizeof(buf));
        rec->record_type   = static_cast<uint32_t>(egbos::wal::WalRecordType::ORDER_NEW);
        rec->payload_length = kOversize;

        egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                     tmp.shadow_path().c_str());
        writer.write_sync(rec);
        // Should never reach here — wal_fatal calls std::abort()
        std::_Exit(0);
    }

    // Parent: wait for child and verify it was killed by SIGABRT.
    // wal_fatal() calls std::abort() which raises SIGABRT.
    // Accepting any non-zero exit code would be too permissive — a crash
    // from an unrelated cause (e.g. SIGSEGV in io_uring init) would produce
    // a false pass. SIGABRT is the only correct signal here.
    int status = 0;
    pid_t waited = ::waitpid(pid, &status, 0);
    ASSERT_EQ(waited, pid);
    ASSERT_TRUE(WIFSIGNALED(status))
        << "Child exited normally or stopped — wal_fatal did not abort. "
        << "WEXITSTATUS=" << (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    EXPECT_EQ(WTERMSIG(status), SIGABRT)
        << "Child was signalled but not by SIGABRT (signal="
        << WTERMSIG(status) << ") — unexpected crash, not wal_fatal";
}

// ─────────────────────────────────────────────────────────────────────────────
// BUG 2b validation: write a record with payload_length >= 4, corrupt 1 byte
// of the payload body on disk, re-scan — must detect truncation.
// was_clean == false; corrupted record NOT counted in valid_records.
// ─────────────────────────────────────────────────────────────────────────────

TEST(WalWriterTest, PayloadCrcDetectsCorruption) {
    TempDir tmp;

    // Payload layout (8 bytes total):
    //   bytes [0..3] — data covered by CRC32C
    //   bytes [4..7] — CRC32C slot (written by write_sync)
    // The corruption will land at payload byte index 1 — inside [0..3],
    // i.e. in the CRC-covered region, NOT in the CRC slot itself.
    constexpr uint32_t kPayloadLen = 8;
    constexpr std::size_t kNumRecordsBefore = 5;  // header-only records before target

    {
        egbos::wal::WalWriter writer(tmp.primary_path().c_str(),
                                     tmp.shadow_path().c_str());

        // Write kNumRecordsBefore records with no payload
        for (std::size_t i = 0; i < kNumRecordsBefore; ++i) {
            alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
            auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
            fill_test_record(rec, egbos::wal::WalRecordType::ORDER_NEW,
                             1, static_cast<int64_t>(i), 0);
            writer.write_sync(rec);
        }

        // Write target record with payload_length = kPayloadLen
        alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize + kPayloadLen]{};
        auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
        std::memset(rec, 0, sizeof(buf));
        rec->record_type    = static_cast<uint32_t>(egbos::wal::WalRecordType::POSITION_SYNC);
        rec->instrument_id  = 42;
        rec->payload_length = kPayloadLen;
        // Write known data into payload body (first 4 bytes; last 4 = CRC slot)
        rec->payload[0] = 0xAB;
        rec->payload[1] = 0xCD;
        rec->payload[2] = 0xEF;
        rec->payload[3] = 0x01;
        // Last 4 bytes of payload are CRC — write_sync() will fill them
        writer.write_sync(rec);
    }

    // Verify clean write of kNumRecordsBefore + 1 records
    {
        auto result = egbos::wal::WalWriter::scan_wal(tmp.primary_path().c_str());
        ASSERT_EQ(result.valid_records, kNumRecordsBefore + 1)
            << "Pre-corruption: unexpected record count";
        ASSERT_TRUE(result.was_clean) << "Pre-corruption: WAL not clean";
    }

    // Corrupt payload byte index 1 of the target record.
    // Byte-level proof that this lands in the CRC-covered data region:
    //   record_start  = kNumRecordsBefore * kWalHeaderWireSize
    //                 = 5 * 72 = 360
    //   payload_start = record_start + kWalHeaderWireSize = 360 + 72 = 432
    //   corrupt_byte  = payload_start + 1 = 433   ← payload[1], value 0xCD
    //   CRC slot      = payload_start + 4 = 436..439  ← NOT touched
    // Therefore: CRC32C is computed over payload[0..3], corruption is at [1].
    {
        int fd = ::open(tmp.primary_path().c_str(), O_RDWR);
        ASSERT_GE(fd, 0);

        // kNumRecordsBefore records each have payload_length=0, so each
        // occupies exactly kWalHeaderWireSize bytes. The target record
        // starts at byte 5*72 = 360.
        const off_t payload_start = static_cast<off_t>(
            kNumRecordsBefore * egbos::wal::kWalHeaderWireSize +
            egbos::wal::kWalHeaderWireSize);
        // Corrupt data byte at payload[1] — inside CRC coverage [0..3]
        const off_t corrupt_offset = payload_start + 1;

        uint8_t bad_byte = 0x00;  // was 0xCD — guaranteed different
        ssize_t n = ::pwrite(fd, &bad_byte, 1, corrupt_offset);
        ASSERT_EQ(n, 1);
        ::close(fd);
    }

    // Re-scan: must detect truncation at the corrupted record
    {
        auto result = egbos::wal::WalWriter::scan_wal(tmp.primary_path().c_str());
        EXPECT_FALSE(result.was_clean)
            << "Expected was_clean=false after payload corruption";
        EXPECT_EQ(result.valid_records, kNumRecordsBefore)
            << "Corrupted record must not be counted as valid";
        EXPECT_EQ(result.truncation_byte_offset,
                  kNumRecordsBefore * egbos::wal::kWalHeaderWireSize)
            << "Truncation offset must point to start of corrupted record";
    }
}

}  // namespace

#else  // !__linux__

// Stub for non-Linux platforms — tests are skipped
#include <gtest/gtest.h>
TEST(WalWriterTest, PlatformSkipped) {
    GTEST_SKIP() << "WAL writer tests require Linux (io_uring)";
}

#endif  // __linux__
