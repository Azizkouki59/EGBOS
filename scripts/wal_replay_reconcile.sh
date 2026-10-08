#!/usr/bin/env bash
# scripts/wal_replay_reconcile.sh — REC-010 step 3
# EGBOS_MASTER.md line 3241 (REC-010, TERMINAL_HALT + exchange connectivity loss)
#
# Performs WAL-internal position reconstruction only. This is deliberately
# independent of exchange connectivity, per the spec: "Do NOT wait for
# exchange connectivity to perform WAL-internal reconciliation."
#
# Does NOT perform RFP reconciliation (step 5) — that requires live
# exchange connectivity and is a separate, manual step once connectivity
# is restored. rfp_reconciled and rfp_delta are always emitted as
# false/PENDING by this tool; only a human process with real exchange
# data may populate them.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build_native"
PREFLIGHT_TOOL="${BUILD_DIR}/egbos_preflight_checks"
WAL_FILE="${REPO_ROOT}/wal_primary.bin"
OUTPUT=""

while [ $# -gt 0 ]; do
    case "$1" in
        --output)
            OUTPUT="$2"
            shift 2
            ;;
        --wal)
            WAL_FILE="$2"
            shift 2
            ;;
        *)
            echo "Usage: $0 [--wal <path>] [--output <path>]" >&2
            exit 1
            ;;
    esac
done

if [ ! -x "${PREFLIGHT_TOOL}" ]; then
    echo "FAIL (${PREFLIGHT_TOOL} not built; run scripts/preflight.sh first)" >&2
    exit 1
fi

if [ -n "${OUTPUT}" ]; then
    "${PREFLIGHT_TOOL}" internal-position-reconstruct "${WAL_FILE}" --output "${OUTPUT}"
else
    "${PREFLIGHT_TOOL}" internal-position-reconstruct "${WAL_FILE}"
fi
