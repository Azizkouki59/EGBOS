#!/usr/bin/env bash
# scripts/run_kill9_wal_test.sh
# EGBOS PROC-KILL9-001 — Reproducible kill-9 injection harness
#
# Specification  : EGBOS-SPEC-S1-001 §15.1 (wal_kill9_survival), §16 (V4), §17.1, §20.2
# Claim          : CLAIM-WAL-001 (wal.durability, V4)
# CI gate        : P3 (unit), integrated into nightly P6 for proof artifact generation
#
# Purpose:
#   Fork a child process that continuously writes WAL records via WalWriter.
#   After a random delay, send SIGKILL to the child (simulating process crash).
#   Run WAL recovery scan on the resulting files.
#   Verify:
#     - CRC32C passes on all recovered records
#     - Sequence is strictly monotonic
#     - At most 1 in-flight record was lost
#     - Writer can restart and continue from the recovered sequence
#
# Pass criterion (§15.1):
#   10/10 trials: recovered state byte-identical to pre-crash state
#   minus at most 1 in-flight record.
#
# Usage:
#   ./scripts/run_kill9_wal_test.sh [--trials N] [--build-dir DIR] [--output-dir DIR]
#
# Environment:
#   EGBOS_HARDWARE_ID  — hardware identifier for proof artifact (default: dev-workstation-001)
#   EGBOS_REPO_ROOT    — repository root (default: auto-detected from script location)
#
# Requirements:
#   - Linux with io_uring support (kernel 5.1+)
#   - GCC 13+ or Clang 17+ (for compiling the child binary)
#   - Built wal_writer_test binary OR compiler available to build child inline

set -euo pipefail

# ── Configuration ─────────────────────────────────────────────────────────────

TRIALS=10
BUILD_DIR=""
OUTPUT_DIR=""
REPO_ROOT="${EGBOS_REPO_ROOT:-}"
HWID="${EGBOS_HARDWARE_ID:-dev-workstation-001}"
DATE_STAMP=$(date +%Y%m%d)
VERBOSE=0

# ── Argument parsing ─────────────────────────────────────────────────────────

usage() {
    echo "Usage: $0 [--trials N] [--build-dir DIR] [--output-dir DIR] [--verbose]"
    echo ""
    echo "  --trials N       Number of kill-9 trials (minimum 10, default: 10)"
    echo "  --build-dir DIR  CMake build directory (default: <repo>/build)"
    echo "  --output-dir DIR Proof artifact output directory"
    echo "  --verbose        Print detailed per-trial output"
    echo ""
    echo "Environment:"
    echo "  EGBOS_HARDWARE_ID   Hardware ID for proof artifact (default: dev-workstation-001)"
    echo "  EGBOS_REPO_ROOT     Repository root (default: auto-detect)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --trials)
            TRIALS="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "ERROR: Unknown argument: $1"
            usage
            ;;
    esac
done

# Enforce minimum 10 trials per CLAIM-WAL-001 enforcement_gaps
if [[ "$TRIALS" -lt 10 ]]; then
    echo "ERROR: Minimum 10 trials required (CLAIM-WAL-001 enforcement_gaps)."
    echo "       Requested: $TRIALS"
    exit 1
fi

# ── Path resolution ──────────────────────────────────────────────────────────

if [[ -z "$REPO_ROOT" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
fi

if [[ -z "$BUILD_DIR" ]]; then
    BUILD_DIR="$REPO_ROOT/build"
fi

if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="$REPO_ROOT/governance/proof_artifacts/wal_kill9_test_results"
fi

mkdir -p "$OUTPUT_DIR"

LOG_FILE="$OUTPUT_DIR/run_${DATE_STAMP}.log"
CHILD_SRC_DIR=$(mktemp -d /tmp/egbos_kill9_build_XXXXXX)
CHILD_BIN="$CHILD_SRC_DIR/wal_kill9_child"

# ── Platform check ───────────────────────────────────────────────────────────

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "ERROR: PROC-KILL9-001 requires Linux (io_uring). Current platform: $(uname -s)"
    exit 1
fi

# ── Compile kill-9 child binary ──────────────────────────────────────────────
#
# The child binary is a minimal WAL writer that:
#   1. Opens a WAL primary + shadow file pair
#   2. Writes records in a tight loop until killed
#   3. On --verify mode: scans the WAL and prints recovery results as key=value
#
# This is compiled from an inline heredoc to avoid adding a CMake target
# solely for the test harness. The child includes engine/core/wal_writer.h
# (header-only) and links nothing beyond the standard library.

cat > "$CHILD_SRC_DIR/wal_kill9_child.cpp" << 'CHILD_EOF'
// wal_kill9_child.cpp — PROC-KILL9-001 child process
// Compiled inline by run_kill9_wal_test.sh
// Usage:
//   wal_kill9_child write <primary_path> <shadow_path> <records_target>
//   wal_kill9_child verify <primary_path>

#include "core/wal_writer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void fill_record(egbos::wal::WalRecord* rec,
                        uint32_t instrument_id,
                        int64_t qty,
                        int64_t price) {
    std::memset(rec, 0, egbos::wal::kWalHeaderWireSize);
    rec->record_type = static_cast<uint32_t>(egbos::wal::WalRecordType::ORDER_NEW);
    rec->instrument_id = instrument_id;
    rec->order_id = 0;
    rec->quantity_delta = qty;
    rec->price = price;
    rec->payload_length = 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s write <primary> <shadow> <records>\n", argv[0]);
        std::fprintf(stderr, "       %s verify <primary>\n", argv[0]);
        return 3;
    }

    const char* mode = argv[1];

    if (std::strcmp(mode, "write") == 0) {
        if (argc < 5) {
            std::fprintf(stderr, "write mode requires: <primary> <shadow> <records_target>\n");
            return 3;
        }
        const char* primary = argv[2];
        const char* shadow = argv[3];
        int records_target = std::atoi(argv[4]);
        if (records_target <= 0) records_target = 100000;

        egbos::wal::WalWriter writer(primary, shadow);

        // Write a SESSION_START record first
        writer.start_session();

        // Write records in a tight loop — the harness will SIGKILL us
        for (int i = 0; i < records_target; ++i) {
            alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize]{};
            auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
            fill_record(rec, 1,
                        static_cast<int64_t>(i),
                        static_cast<int64_t>(i) * egbos::wal::kPriceScale);
            writer.write_sync(rec);

            // Print progress marker every 1000 records for harness synchronisation
            if ((i + 1) % 1000 == 0) {
                std::printf("PROGRESS:%d\n", i + 1);
                std::fflush(stdout);
            }
        }

        // If we reach here, the harness didn't kill us fast enough
        std::printf("COMPLETED:%d\n", records_target);
        std::fflush(stdout);
        return 0;

    } else if (std::strcmp(mode, "verify") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "verify mode requires: <primary>\n");
            return 3;
        }
        const char* primary = argv[2];

        auto result = egbos::wal::WalWriter::scan_wal(primary);

        std::printf("valid_records=%lu\n", result.valid_records);
        std::printf("was_clean=%d\n", result.was_clean ? 1 : 0);
        std::printf("last_valid_sequence=%lu\n", result.last_valid_sequence);
        std::printf("truncation_byte_offset=%lu\n", result.truncation_byte_offset);

        // Verify CRC32C on every record by re-reading
        int fd = open(primary, O_RDONLY);
        if (fd < 0) {
            std::fprintf(stderr, "ERROR: Cannot open %s for CRC verification\n", primary);
            return 1;
        }

        uint64_t offset = 0;
        uint64_t crc_pass = 0;
        uint64_t crc_fail = 0;
        uint64_t prev_seq = 0;
        bool sequence_monotonic = true;

        for (uint64_t i = 0; i < result.valid_records; ++i) {
            alignas(8) uint8_t buf[egbos::wal::kWalHeaderWireSize];
            ssize_t n = pread(fd, buf, egbos::wal::kWalHeaderWireSize,
                              static_cast<off_t>(offset));
            if (n != static_cast<ssize_t>(egbos::wal::kWalHeaderWireSize)) {
                std::fprintf(stderr, "ERROR: Short read at record %lu\n", i);
                crc_fail++;
                break;
            }

            auto* rec = reinterpret_cast<egbos::wal::WalRecord*>(buf);
            uint32_t computed = egbos::wal::detail::compute_crc32c(
                buf, egbos::wal::kWalCrcCoverageBytes);

            if (computed == rec->header_crc32c) {
                crc_pass++;
            } else {
                crc_fail++;
            }

            // Check sequence monotonicity (first record may be 0 for SESSION_START)
            if (i > 0 && rec->sequence <= prev_seq) {
                sequence_monotonic = false;
            }
            prev_seq = rec->sequence;

            offset += egbos::wal::kWalHeaderWireSize + rec->payload_length;
        }

        close(fd);

        std::printf("crc_pass=%lu\n", crc_pass);
        std::printf("crc_fail=%lu\n", crc_fail);
        std::printf("sequence_monotonic=%d\n", sequence_monotonic ? 1 : 0);

        // Verify writer can restart from recovery point
        // (Open a new writer on the same files and check sequence continues)
        if (result.valid_records > 0) {
            const char* shadow_verify = "/dev/null";
            // We create a temporary shadow for restart verification
            char shadow_tmp[] = "/tmp/egbos_kill9_shadow_XXXXXX";
            int shadow_fd = mkstemp(shadow_tmp);
            if (shadow_fd >= 0) {
                close(shadow_fd);
                egbos::wal::WalWriter restart_writer(primary, shadow_tmp);
                uint64_t restart_seq = restart_writer.current_sequence();
                std::printf("restart_sequence=%lu\n", restart_seq);
                std::printf("restart_continues=%d\n",
                            (restart_seq == result.valid_records) ? 1 : 0);
                unlink(shadow_tmp);
            }
        }

        return (crc_fail == 0 && sequence_monotonic) ? 0 : 1;
    }

    std::fprintf(stderr, "ERROR: Unknown mode '%s'\n", mode);
    return 3;
}
CHILD_EOF

echo "═══════════════════════════════════════════════════════════════════════"
echo "  EGBOS PROC-KILL9-001 — WAL kill-9 Survival Test Harness"
echo "  Specification: §15.1 wal_kill9_survival, CLAIM-WAL-001 (V4)"
echo "═══════════════════════════════════════════════════════════════════════"
echo ""
echo "  Trials requested : $TRIALS"
echo "  Hardware ID      : $HWID"
echo "  Repository root  : $REPO_ROOT"
echo "  Build directory  : $BUILD_DIR"
echo "  Output directory : $OUTPUT_DIR"
echo "  Log file         : $LOG_FILE"
echo ""

# Compile the child binary
echo "[BUILD] Compiling wal_kill9_child..."

CXX="${CXX:-g++}"

# Detect compiler
if ! command -v "$CXX" &>/dev/null; then
    if command -v g++-13 &>/dev/null; then
        CXX="g++-13"
    elif command -v clang++-17 &>/dev/null; then
        CXX="clang++-17"
    else
        echo "ERROR: No suitable C++ compiler found. Set CXX environment variable."
        exit 1
    fi
fi

echo "[BUILD] Using compiler: $CXX"

"$CXX" -std=c++20 -O2 \
    -I "$REPO_ROOT/engine" \
    -o "$CHILD_BIN" \
    "$CHILD_SRC_DIR/wal_kill9_child.cpp" \
    -lpthread 2>&1

if [[ ! -x "$CHILD_BIN" ]]; then
    echo "ERROR: Failed to compile wal_kill9_child"
    exit 1
fi

echo "[BUILD] Child binary compiled: $CHILD_BIN"
echo ""

# ── Trial execution ──────────────────────────────────────────────────────────

PASS_COUNT=0
FAIL_COUNT=0
TOTAL_RECORDS_WRITTEN=0
TOTAL_RECORDS_RECOVERED=0

# Initialise log file
cat > "$LOG_FILE" << EOF
# EGBOS PROC-KILL9-001 — WAL kill-9 Survival Test Results
# Date:        $(date -Iseconds)
# Hardware ID: $HWID
# Kernel:      $(uname -r)
# Compiler:    $($CXX --version | head -n1)
# Trials:      $TRIALS
# Claim:       CLAIM-WAL-001 (wal.durability, V4)
# Spec ref:    §15.1 wal_kill9_survival
# Pass criterion: 10/10 trials; recovered state minus at most 1 in-flight record.
#
# Format: TRIAL <n> | VERDICT <PASS|FAIL> | KILLED_AFTER <ms> | RECORDS_WRITTEN <n> | RECORDS_RECOVERED <n> | RECORDS_LOST <n> | CRC_PASS <n> | CRC_FAIL <n> | SEQ_MONOTONIC <0|1> | RESTART_CONTINUES <0|1>
EOF

echo "───────────────────────────────────────────────────────────────────────"
echo "  Running $TRIALS kill-9 trials..."
echo "───────────────────────────────────────────────────────────────────────"

for trial in $(seq 1 "$TRIALS"); do
    TRIAL_DIR=$(mktemp -d /tmp/egbos_kill9_trial_XXXXXX)
    PRIMARY="$TRIAL_DIR/wal_primary.bin"
    SHADOW="$TRIAL_DIR/wal_shadow.bin"

    # Random kill delay: 5ms to 500ms (enough for some records to be written)
    # $RANDOM is 0-32767; scale to 5000-500000 microseconds
    KILL_DELAY_US=$(( (RANDOM % 495000) + 5000 ))
    KILL_DELAY_MS=$(( KILL_DELAY_US / 1000 ))

    # Records target: high enough that the child won't finish before kill
    RECORDS_TARGET=1000000

    # Launch the child process
    "$CHILD_BIN" write "$PRIMARY" "$SHADOW" "$RECORDS_TARGET" > "$TRIAL_DIR/child_stdout.txt" 2>&1 &
    CHILD_PID=$!

    # Wait for random delay, then SIGKILL
    usleep "$KILL_DELAY_US" 2>/dev/null || sleep "0.$(printf '%03d' $KILL_DELAY_MS)"

    # SIGKILL — the core of PROC-KILL9-001
    kill -9 "$CHILD_PID" 2>/dev/null || true
    wait "$CHILD_PID" 2>/dev/null || true

    # Count records written (from progress markers and file size)
    RECORDS_WRITTEN=0
    if [[ -f "$TRIAL_DIR/child_stdout.txt" ]]; then
        LAST_PROGRESS=$(grep -oP 'PROGRESS:\K\d+' "$TRIAL_DIR/child_stdout.txt" | tail -1 || echo "0")
        RECORDS_WRITTEN="${LAST_PROGRESS:-0}"
    fi
    # Also estimate from file size
    if [[ -f "$PRIMARY" ]]; then
        FILE_SIZE=$(stat -c%s "$PRIMARY" 2>/dev/null || echo "0")
        RECORDS_FROM_SIZE=$(( FILE_SIZE / 72 ))  # kWalHeaderWireSize = 72
        if [[ "$RECORDS_FROM_SIZE" -gt "$RECORDS_WRITTEN" ]]; then
            RECORDS_WRITTEN=$RECORDS_FROM_SIZE
        fi
    fi

    # Run recovery verification
    VERIFY_OUTPUT=$("$CHILD_BIN" verify "$PRIMARY" 2>&1) || true

    # Parse verification results
    VALID_RECORDS=$(echo "$VERIFY_OUTPUT" | grep -oP 'valid_records=\K\d+' || echo "0")
    WAS_CLEAN=$(echo "$VERIFY_OUTPUT" | grep -oP 'was_clean=\K\d+' || echo "0")
    CRC_PASS=$(echo "$VERIFY_OUTPUT" | grep -oP 'crc_pass=\K\d+' || echo "0")
    CRC_FAIL=$(echo "$VERIFY_OUTPUT" | grep -oP 'crc_fail=\K\d+' || echo "0")
    SEQ_MONOTONIC=$(echo "$VERIFY_OUTPUT" | grep -oP 'sequence_monotonic=\K\d+' || echo "0")
    RESTART_CONTINUES=$(echo "$VERIFY_OUTPUT" | grep -oP 'restart_continues=\K\d+' || echo "0")

    # Calculate records lost
    RECORDS_LOST=0
    if [[ "$RECORDS_WRITTEN" -gt 0 ]]; then
        RECORDS_LOST=$(( RECORDS_WRITTEN - VALID_RECORDS ))
        if [[ "$RECORDS_LOST" -lt 0 ]]; then
            RECORDS_LOST=0
        fi
    fi

    # Determine verdict
    # Pass if: CRC all pass, sequence monotonic, at most 1 record lost, restart continues
    VERDICT="FAIL"
    if [[ "$CRC_FAIL" -eq 0 ]] && \
       [[ "$SEQ_MONOTONIC" -eq 1 ]] && \
       [[ "$RECORDS_LOST" -le 1 ]] && \
       [[ "$RESTART_CONTINUES" -eq 1 || "$VALID_RECORDS" -eq 0 ]]; then
        VERDICT="PASS"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi

    TOTAL_RECORDS_WRITTEN=$((TOTAL_RECORDS_WRITTEN + RECORDS_WRITTEN))
    TOTAL_RECORDS_RECOVERED=$((TOTAL_RECORDS_RECOVERED + VALID_RECORDS))

    # Log result
    echo "TRIAL $trial | VERDICT $VERDICT | KILLED_AFTER ${KILL_DELAY_MS}ms | RECORDS_WRITTEN $RECORDS_WRITTEN | RECORDS_RECOVERED $VALID_RECORDS | RECORDS_LOST $RECORDS_LOST | CRC_PASS $CRC_PASS | CRC_FAIL $CRC_FAIL | SEQ_MONOTONIC $SEQ_MONOTONIC | RESTART_CONTINUES $RESTART_CONTINUES" >> "$LOG_FILE"

    # Console output
    if [[ "$VERDICT" == "PASS" ]]; then
        printf "  Trial %2d/%d: PASS  (killed@%3dms, written=%d, recovered=%d, lost=%d)\n" \
            "$trial" "$TRIALS" "$KILL_DELAY_MS" "$RECORDS_WRITTEN" "$VALID_RECORDS" "$RECORDS_LOST"
    else
        printf "  Trial %2d/%d: FAIL  (killed@%3dms, written=%d, recovered=%d, lost=%d, crc_fail=%d, seq_mono=%d, restart=%d)\n" \
            "$trial" "$TRIALS" "$KILL_DELAY_MS" "$RECORDS_WRITTEN" "$VALID_RECORDS" "$RECORDS_LOST" \
            "$CRC_FAIL" "$SEQ_MONOTONIC" "$RESTART_CONTINUES"
    fi

    if [[ "$VERBOSE" -eq 1 ]]; then
        echo "    Verify output: $VERIFY_OUTPUT"
    fi

    # Cleanup trial directory
    rm -rf "$TRIAL_DIR"
done

# ── Summary ──────────────────────────────────────────────────────────────────

echo ""
echo "───────────────────────────────────────────────────────────────────────"
echo "  PROC-KILL9-001 Summary"
echo "───────────────────────────────────────────────────────────────────────"
echo "  Trials: $TRIALS"
echo "  Passed: $PASS_COUNT"
echo "  Failed: $FAIL_COUNT"
echo "  Total records written:   $TOTAL_RECORDS_WRITTEN"
echo "  Total records recovered: $TOTAL_RECORDS_RECOVERED"
echo ""

# Append summary to log
cat >> "$LOG_FILE" << EOF

# ── Summary ──
# Trials:    $TRIALS
# Passed:    $PASS_COUNT
# Failed:    $FAIL_COUNT
# Total records written:   $TOTAL_RECORDS_WRITTEN
# Total records recovered: $TOTAL_RECORDS_RECOVERED
# Hardware ID: $HWID
# Kernel:      $(uname -r)
# Date:        $(date -Iseconds)
#
# OVERALL_VERDICT: $(if [[ "$FAIL_COUNT" -eq 0 ]]; then echo "PASS"; else echo "FAIL"; fi)
EOF

echo "  Log file: $LOG_FILE"
echo ""

# Cleanup build directory
rm -rf "$CHILD_SRC_DIR"

# Final verdict
if [[ "$FAIL_COUNT" -eq 0 ]]; then
    echo "═══════════════════════════════════════════════════════════════════════"
    echo "  PROC-KILL9-001: PASS ($PASS_COUNT/$TRIALS trials)"
    echo "  CLAIM-WAL-001 kill-9 survival criterion: SATISFIED"
    echo "═══════════════════════════════════════════════════════════════════════"
    exit 0
else
    echo "═══════════════════════════════════════════════════════════════════════"
    echo "  PROC-KILL9-001: FAIL ($FAIL_COUNT/$TRIALS trials failed)"
    echo "  CLAIM-WAL-001 kill-9 survival criterion: NOT SATISFIED"
    echo "═══════════════════════════════════════════════════════════════════════"
    exit 1
fi
