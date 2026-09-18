#!/usr/bin/env bash
# scripts/run_s5_market_data.sh
# S5 Market Data Qualification Gate Script
#
# EGBOS Institutional Specification S5-INST-SPEC-001
#   S5-REQ-001: CLAIM-L3-05 at V4 â€” startup buffer sized against live feed jitter
#   S5-REQ-002: CLAIM-L0-002 at V4 â€” gap detection on real exchange sequences
#   S5-REQ-005: Zero Order Path â€” no outbound FIX or ExecCommand path
#
# What this measures:
#   Dual-threaded UDP multicast loopback simulation:
#     Publisher: 50k msg/s with 1/64 gap injection, up to 500Âµs jitter,
#                and a deliberate 600ms stale pause (> kStaleThresholdNs 500ms)
#     Listener:  FeedIngestor state-machine covering FEED_NORMAL, GAP_DETECTED,
#                FEED_STALE transitions. SpscRing<65536> high-water mark tracked.
#
# Pass conditions (all must hold):
#   gap_detection_accuracy = 100%   -> proves P1 gap-detection criterion
#   stale_feed_detected    = true   -> proves P2 stale-detection criterion
#   ring_hwm_pct           < 90%    -> proves P3 Ring HWM criterion (buffer never overwhelmed)
#
# Govcheck compliance:
#   GC-E-010: no malloc/new on hot path (stack-allocated UDP buffer, static ring)
#   GC-E-011: CLOCK_MONOTONIC only on hot path
#   GC-E-042: no business logic in listener path â€” pure state-machine + ring drain
#   S5-REQ-005: TX socket is loopback-only. No FIX encoder. No ExecCommand dispatch.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT_DIR="${REPO_ROOT}/governance/proof_artifacts"
ARTIFACT_PATH="${ARTIFACT_DIR}/s5_market_data_results.json"
HARNESS_SRC="${REPO_ROOT}/scripts/s5_live_feed_harness.cpp"
HARNESS_BIN="${REPO_ROOT}/build/s5_live_feed_harness"

echo "=== EGBOS S5 Market Data Qualification Gate ==="
echo "S5-INST-SPEC-001: P1/P2/P3 synthetic market-data criteria + S5-REQ-005 boundary"

mkdir -p "${ARTIFACT_DIR}"
mkdir -p "$(dirname "${HARNESS_BIN}")"

# â”€â”€ Step 1: Verify source harness exists â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
if [ ! -f "${HARNESS_SRC}" ]; then
    echo "ERROR: S5 harness source not found: ${HARNESS_SRC}"
    exit 1
fi
echo "[1/4] Source: ${HARNESS_SRC} âœ“"

# â”€â”€ Step 2: Build harness â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo "[2/4] Building S5 live feed harness..."

# Required: C++20 for [[likely]]/[[unlikely]] annotations matching the ingestor.
# -O2 with -DNDEBUG to match production build flags.
# Includes: engine/core (SpscRing), engine/l0 (FeedIngestor, MarketEventSlot stub).
g++ -std=c++20 -O2 -DNDEBUG \
    -I"${REPO_ROOT}" \
    -I"${REPO_ROOT}/engine" \
    "${HARNESS_SRC}" \
    -o "${HARNESS_BIN}" \
    -lpthread

echo "      Built: ${HARNESS_BIN}"

# â”€â”€ Step 3: Run harness â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo "[3/4] Running 60s S5 simulation (gap injection + jitter + stale pause)..."
echo "      Multicast group: 239.0.0.1:20001 (loopback)"
echo "      Gap divisor: 1/64 datagrams"
echo "      Max jitter: 500Âµs"
echo "      Stale pause: 600ms (> kStaleThresholdNs 500ms)"
echo ""

# Change to repo root so relative path for artifact write is correct
pushd "${REPO_ROOT}" > /dev/null
"${HARNESS_BIN}" 2>&1
EXIT_CODE=$?
popd > /dev/null

if [ ${EXIT_CODE} -ne 0 ]; then
    echo ""
    echo "S5 Gate: FAIL (harness exited with code ${EXIT_CODE})"
    exit 1
fi

# â”€â”€ Step 4: Parse and validate artifact â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
echo ""
echo "[4/4] Validating S5 proof artifact..."

if [ ! -f "${ARTIFACT_PATH}" ]; then
    echo "ERROR: Artifact not written: ${ARTIFACT_PATH}"
    exit 1
fi

# Extract verdict from JSON
STATUS=$(python3 -c "
import json, sys
with open('${ARTIFACT_PATH}') as f:
    d = json.load(f)

status = d.get('status', 'UNKNOWN')
m      = d.get('measurements', {})
pass_criteria = d.get('pass_criteria', {})
p1 = pass_criteria.get('P1_gap_accuracy_100pct', {})
p2 = pass_criteria.get('P2_stale_detected', {})
p3 = pass_criteria.get('P3_ring_hwm_lt_90pct', {})

gap_acc  = m.get('gap_detection_accuracy_pct', 0)
stale    = m.get('stale_feed_detected', False)
hwm_pct  = m.get('ring_hwm_pct', 100)
l305_pass = p3.get('result', False)
l002_pass = p1.get('result', False) and p2.get('result', False)

print(f'  Status:                {status}')
print(f'  Gap accuracy:          {gap_acc:.2f}%')
print(f'  Stale detected:        {stale}')
print(f'  Ring HWM pct:          {hwm_pct:.2f}%')
print('  P3 Ring HWM criterion:        ' + ('PASS' if l305_pass else 'FAIL'))
print('  P1/P2 Gap+Stale criteria:     ' + ('PASS' if l002_pass else 'FAIL'))

if status != 'PASS':
    print(f'VERDICT: FAIL')
    sys.exit(1)
print(f'VERDICT: PASS')
")
echo "${STATUS}"

echo ""
echo "S5 Simulation: PASS"
echo "Artifact: ${ARTIFACT_PATH}"
echo ""
echo "No automatic claim promotion is performed by this pipeline."
echo "CLAIM-L3-05 and CLAIM-L0-002 remain unchanged; governance approval is required for any class change."
exit 0
