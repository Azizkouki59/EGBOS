#!/usr/bin/env bash
# scripts/run_p7_staging_smoke.sh
# P7 Staging Smoke Gate Validation Script
#
# EGBOS Master Blueprint §B1.4.2 — P7 Gate
#   "60s simulator smoke, P99 within 20% baseline | Block staging deploy"
#   (Blueprint line 1130)
#
# What this measures:
#   Round-trip latency of ExecCommand push (producer) + pop (consumer) through
#   the egbos_exec_commands SPSC ring (L3→L5) — the core P7 timing claim.
#   Baseline: timing_constants.json → spsc_push_pop_p99_ns
#   Pass condition: measured P99 ≤ baseline × 1.20
#
# Govcheck compliance:
#   GC-E-010: harness uses no malloc on hot path (stack-only ring)
#   GC-E-011: CLOCK_MONOTONIC only — CLOCK_REALTIME not used
#   GC-E-042: no business logic in routing path — pure push/pop measurement

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_DIR="${REPO_ROOT}/governance/proof_artifacts"
ARTIFACT_PATH="${ARTIFACT_DIR}/p7_staging_smoke_results.json"
TIMING_FILE="${ARTIFACT_DIR}/timing_constants.json"
HARNESS_SRC="${REPO_ROOT}/scripts/p7_smoke_harness.cpp"
HARNESS_BIN="${REPO_ROOT}/build/p7_smoke_harness"

echo "=== EGBOS P7 Staging Smoke Gate ==="
echo "Blueprint §B1.4.2 — 60s SPSC latency smoke against timing_constants.json baseline"

mkdir -p "${ARTIFACT_DIR}"
mkdir -p "$(dirname "${HARNESS_BIN}")"

# ── Step 1: Verify baseline file exists ──────────────────────────────────────
if [ ! -f "${TIMING_FILE}" ]; then
    echo "ERROR: timing_constants.json not found at ${TIMING_FILE}"
    exit 1
fi

echo "[1/3] Baseline: ${TIMING_FILE}"
BASELINE_P99=$(python3 -c "
import json, sys
d = json.load(open('${TIMING_FILE}'))
v = d.get('measurements', {}).get('spsc_push_pop_p99_ns', 0)
if not v:
    sys.exit(1)
print(v)
")
echo "      spsc_push_pop_p99_ns = ${BASELINE_P99} ns"
echo "      Pass threshold        = $((BASELINE_P99 * 12 / 10)) ns (baseline × 1.20)"

# ── Step 2: Build harness ────────────────────────────────────────────────────
echo "[2/3] Building P7 smoke harness..."
g++ -std=c++20 -O2 -DNDEBUG \
    -I"${REPO_ROOT}" \
    "${HARNESS_SRC}" \
    -o "${HARNESS_BIN}" \
    -lpthread
echo "      Built: ${HARNESS_BIN}"

# ── Step 3: Run harness ──────────────────────────────────────────────────────
echo "[3/3] Running 60s smoke test..."
"${HARNESS_BIN}" "${ARTIFACT_PATH}" "${TIMING_FILE}"
EXIT_CODE=$?

if [ ${EXIT_CODE} -ne 0 ]; then
    echo "P7 Gate: FAIL"
    exit 1
fi

echo "P7 Gate: PASS"
echo "Artifact: ${ARTIFACT_PATH}"
exit 0
