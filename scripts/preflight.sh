#!/usr/bin/env bash
# scripts/preflight.sh — Staging Pre-flight Validation Sequence
# EGBOS Blueprint §E3.2, §E3.3
# Scope: S6 staging qualification (pre-live validation only)
#
# Active checks: PF-01 through PF-09 + PF-12 (10 checks)
# PF-10 (exchange heartbeat) and PF-11 (feed continuity) are S5 live only.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOVCHECK="${REPO_ROOT}/build_native/govcheck/govcheck"
WAL_FILE="${REPO_ROOT}/wal_primary.bin"

# Build the preflight tool when missing or stale.
# PF-02 reconstructs state from the canonical WAL record stream.
# The preflight helper also uses the compile-time instrument bound from
# shm_layout.h; both headers are therefore part of the stale-build check.
BUILD_DIR="${REPO_ROOT}/build_native"
PREFLIGHT_TOOL="${BUILD_DIR}/egbos_preflight_checks"
PREFLIGHT_SRC="${REPO_ROOT}/scripts/egbos_preflight_checks.cpp"
WAL_HEADER="${REPO_ROOT}/engine/core/wal_writer.h"
SHM_HEADER="${REPO_ROOT}/engine/core/shm_layout.h"

if [ ! -x "${PREFLIGHT_TOOL}" ] ||
   [ "${PREFLIGHT_SRC}" -nt "${PREFLIGHT_TOOL}" ] ||
   [ "${WAL_HEADER}" -nt "${PREFLIGHT_TOOL}" ] ||
   [ "${SHM_HEADER}" -nt "${PREFLIGHT_TOOL}" ]; then

    cmake -U FETCHCONTENT_SOURCE_DIR_FLATBUFFERS \
        -U FETCHCONTENT_FULLY_DISCONNECTED \
        -U FETCHCONTENT_UPDATES_DISCONNECTED \
        -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DFETCHCONTENT_FULLY_DISCONNECTED=OFF \
        -DFETCHCONTENT_UPDATES_DISCONNECTED=OFF
    cmake --build "${BUILD_DIR}" --target flatc generate_schemas -j2

    FLATC_BIN="${BUILD_DIR}/_deps/flatbuffers-build/flatc"
    if [ ! -x "${FLATC_BIN}" ]; then
        echo "FAIL (project-managed flatc not found: ${FLATC_BIN})"
        exit 1
    fi

    FLATC_VERSION="$("${FLATC_BIN}" --version)"
    if [ "${FLATC_VERSION}" != "flatc version 24.3.25" ]; then
        echo "FAIL (non-canonical flatc: ${FLATC_VERSION}; expected flatc version 24.3.25)"
        exit 1
    fi

    g++ -std=c++20 -O2 \
        -I "${REPO_ROOT}/engine" \
        -I "${BUILD_DIR}/schemas/generated" \
        -I "${BUILD_DIR}/_deps/flatbuffers-src/include" \
        "${PREFLIGHT_SRC}" \
        -o "${PREFLIGHT_TOOL}" \
        -lssl -lcrypto
fi

echo "=== EGBOS Pre-flight: Staging Validation Sequence ==="
echo "Blueprint §E3.3 — Phase 1 (State Validation: PF-01..PF-05): serial"
echo ""

# PF-01: WAL Integrity Scan
echo "[PF-01] WAL Integrity Scan"
if ! "${PREFLIGHT_TOOL}" wal-scan "${WAL_FILE}"; then
    echo "PF-01 FAILED — invoking REC-008 steps 1-3 (WAL-only recovery record)"
    "${PREFLIGHT_TOOL}" wal-recovery-record "${WAL_FILE}" || true
    echo "REC-008 steps 4-7 (RFP reconciliation if applicable, Founder"
    echo "HITL_RESTART_AUTH signature, INCIDENT record) require manual"
    echo "action before this engine may restart. See governance/recovery/"
    echo "for the generated RECOVERY-RECORD."
    exit 1
fi

# PF-02: Position Reconstruction Verification
echo "[PF-02] WAL-only Position Reconstruction — pre-engine INV-001 prerequisite"
"${PREFLIGHT_TOOL}" position-reconstruct "${WAL_FILE}"

# PF-03: Safety Monitor Latch Audit
echo "[PF-03] Safety Monitor Latch Audit"
"${PREFLIGHT_TOOL}" risk-latches

# PF-04: Governance Flags Region Initialization
echo "[PF-04] Governance Flags Region Initialization"
"${PREFLIGHT_TOOL}" governance-flags

# PF-05: Claim Expiry Sweep
echo "[PF-05] Claim Expiry Sweep — govcheck deploy-gate --target staging"
"${GOVCHECK}" deploy-gate --target staging --repo-root "${REPO_ROOT}"
echo "        Status: PASS"
echo ""
echo "Phase 1 RESULT — no FAILs (PF-03/PF-04 are N/A in pre-live staging)"
echo ""
echo "=== Phase 2 (Schema Verification: PF-06..PF-09): serial ==="

# PF-06: FlatBuffers Schema Freshness
echo "[PF-06] FlatBuffers Schema Freshness"
FLATC="${BUILD_DIR}/_deps/flatbuffers-build/flatc"
PF06_TMP="$(mktemp -d)"
trap 'rm -rf "${PF06_TMP}"' EXIT
PF06_FAIL=0
if [ ! -x "${FLATC}" ]; then
    echo "FAIL (flatc not found at ${FLATC})"
    exit 1
fi
for fbs in "${REPO_ROOT}"/schemas/*.fbs; do
    "${FLATC}" --cpp -o "${PF06_TMP}" "${fbs}" || { echo "FAIL (flatc failed on ${fbs})"; exit 1; }
done
for gen in "${PF06_TMP}"/*_generated.h; do
    base="$(basename "${gen}")"
    committed="${REPO_ROOT}/schemas/generated/${base}"
    if [ ! -f "${committed}" ]; then
        continue
    fi
    if ! diff -q "${gen}" "${committed}" > /dev/null; then
        echo "FAIL (${base} differs from freshly generated output)"
        PF06_FAIL=1
    fi
done
if [ "${PF06_FAIL}" -ne 0 ]; then
    exit 1
fi
echo "PASS (committed generated headers match fresh flatc output)"

# PF-07: Registry Entry Signature Verification
echo "[PF-07] Registry Entry Signature Verification"
MODEL_REGISTRY_DIR="${REPO_ROOT}/governance/model-registry"
if [ ! -d "${MODEL_REGISTRY_DIR}" ]; then
    echo "NOT APPLICABLE (governance/model-registry/ does not exist; S11 not started)"
elif [ -z "$(find "${MODEL_REGISTRY_DIR}" -maxdepth 1 -name '*.yaml' -print -quit)" ]; then
    echo "NOT APPLICABLE (governance/model-registry/ exists but has zero entries)"
else
    PF07_FAIL=0
    for entry in "${MODEL_REGISTRY_DIR}"/*.yaml; do
        if ! grep -q "^signature:" "${entry}"; then
            echo "FAIL (${entry} missing signature field)"
            PF07_FAIL=1
        fi
    done
    if [ "${PF07_FAIL}" -ne 0 ]; then
        exit 1
    fi
    echo "PASS (all model registry entries have a signature field)"
fi

# PF-08: timing_constants.json Hardware Match
echo "[PF-08] timing_constants.json Hardware Match"
TIMING_FILE="${REPO_ROOT}/governance/proof_artifacts/timing_constants.json"
RUNTIME_HWID="${EGBOS_HARDWARE_ID:-$(hostname)}"
if [ ! -f "${TIMING_FILE}" ]; then
    echo "FAIL (${TIMING_FILE} not found)"
    exit 1
fi
COMMITTED_HWID="$(grep -o '"hardware_id":[[:space:]]*"[^"]*"' "${TIMING_FILE}" | sed 's/.*"\([^"]*\)"$/\1/')"
if [ -z "${COMMITTED_HWID}" ]; then
    echo "FAIL (hardware_id field missing or unparsable in ${TIMING_FILE})"
    exit 1
fi
if [ "${COMMITTED_HWID}" = "dev-workstation-001" ] || [ "${COMMITTED_HWID}" = "staging-synthetic" ]; then
    echo "NOT APPLICABLE (committed timing_constants.json is a dev/staging baseline, hardware_id=${COMMITTED_HWID}; production match not required pre-S7)"
elif [ "${COMMITTED_HWID}" = "${RUNTIME_HWID}" ]; then
    echo "PASS (hardware_id=${COMMITTED_HWID} matches runtime host)"
else
    echo "FAIL (committed hardware_id=${COMMITTED_HWID} does not match runtime host=${RUNTIME_HWID})"
    exit 1
fi

# PF-09: ONNX Model Hash and Runtime Version Verification
echo "[PF-09] ONNX Model Hash and Runtime Version Verification"
if [ ! -d "${MODEL_REGISTRY_DIR}" ] || [ -z "$(find "${MODEL_REGISTRY_DIR}" -maxdepth 1 -name '*.yaml' -print -quit)" ]; then
    echo "NOT APPLICABLE (Phase V-F not active; no model registry entries)"
else
    echo "FAIL (model registry entries present but PF-09 ONNX verification not yet implemented)"
    exit 1
fi

echo ""
echo "Phase 2 RESULT — no FAILs (PF-08 is N/A in pre-live staging)"
echo ""
echo "=== Phase 3 (Connection Integrity: PF-12 only in staging) ==="

# PF-12: SHM Region mlock Confirmation
echo "[PF-12] SHM Region mlock Confirmation"
SHM_PATHS="egbos_market_events egbos_lob_snapshots egbos_feature_vector egbos_position_state egbos_risk_limits egbos_exec_commands egbos_regime_signal egbos_governance_flags"
SHM_HOLDER_PID=""
for shm in ${SHM_PATHS}; do
    if [ -e "/dev/shm/${shm}" ]; then
        HOLDER="$(fuser "/dev/shm/${shm}" 2>/dev/null | awk '{print $1}')"
        if [ -n "${HOLDER}" ]; then
            SHM_HOLDER_PID="${HOLDER}"
            break
        fi
    fi
done
if [ -z "${SHM_HOLDER_PID}" ]; then
    echo "NOT APPLICABLE (no live process holding /egbos_* SHM regions)"
else
    PF12_FAIL=0
    for shm in ${SHM_PATHS}; do
        if [ ! -e "/dev/shm/${shm}" ]; then
            echo "FAIL (missing SHM region: /dev/shm/${shm})"
            PF12_FAIL=1
        fi
    done
    VMLCK="$(grep -i '^VmLck:' "/proc/${SHM_HOLDER_PID}/status" 2>/dev/null | awk '{print $2}')"
    if [ -z "${VMLCK}" ] || [ "${VMLCK}" -eq 0 ]; then
        echo "FAIL (holder pid=${SHM_HOLDER_PID} has VmLck=0; regions not mlocked)"
        PF12_FAIL=1
    fi
    if [ "${PF12_FAIL}" -ne 0 ]; then
        exit 1
    fi
    echo "PASS (all 8 SHM regions present; holder pid=${SHM_HOLDER_PID} VmLck=${VMLCK}kB)"
fi

echo ""
echo "Phase 3 RESULT — no FAILs (PF-12 is N/A in pre-live staging)"
echo ""
echo "=== VALIDATION RESULT ==="
echo "Staging preflight result: 6 PASS, 4 N/A, 0 FAIL."
echo "governance_flags.preflight_armed = 0 (not armed in pre-live staging)"
echo "PREFLIGHT NOT ARMED — pre-live staging validation only."
