#!/usr/bin/env bash
# scripts/s5_336h_runner.sh
# EGBOS S5-REQ-003 â€” 336-Hour Uninterrupted Trial Infrastructure
#
# Classification  : INFRASTRUCTURE_VALIDATION
# Evidence class  : NOT LIVE_OPERATIONAL until a real 336-hour physical trial completes.
# Governance      : V3 AUTHORITATIVE â€” S5-REQ-003
#
# This script implements the trial state machine:
#
#   TRIAL_STARTED  â†’  TRIAL_CONTINUOUS  â†’  TRIAL_COMPLETED
#                             â†“
#                        TRIAL_FAILED   (on any interruption)
#
# CRITICAL: NO AUTOMATIC RESTART. A process restart invalidates the trial.
#
# Usage:
#   s5_336h_runner.sh [--engine-pid <PID>] [--dry-run-seconds <N>]
#
# --engine-pid <PID>       : Monitor an already-running engine process.
# --dry-run-seconds <N>    : Short-duration test mode. Proves state machine only.
#                            N seconds is the trial duration instead of 336 hours.
#
# Evidence artifact:
#   /var/log/egbos/s5_trial_<TRIAL_ID>.jsonl   (append-only)
#   SHA-256 digest at completion.

set -euo pipefail

# â”€â”€â”€ Configuration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
readonly TRIAL_DURATION_HOURS=336
readonly TRIAL_DURATION_SECS=$((TRIAL_DURATION_HOURS * 3600))
readonly WATCHDOG_INTERVAL_SECS=30
readonly INV001_CHECK_INTERVAL_SECS=60
readonly TELEMETRY_DIR="${EGBOS_TELEMETRY_DIR:-/var/log/egbos}"
readonly TRIAL_ID="$(date -u +%Y%m%dT%H%M%SZ)_$$"

# â”€â”€â”€ State machine â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
TRIAL_STATE="TRIAL_STARTED"
ENGINE_PID=""
BOOT_ID=""
DRY_RUN_SECS=0
TELEMETRY_FILE=""
TRIAL_START_MONO=0

# â”€â”€â”€ Argument parsing â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
while [[ $# -gt 0 ]]; do
    case "$1" in
        --engine-pid)   ENGINE_PID="$2"; shift 2 ;;
        --dry-run-seconds) DRY_RUN_SECS="$2"; shift 2 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# â”€â”€â”€ Utilities â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
mono_now_secs() {
    # CLOCK_MONOTONIC via /proc/uptime â€” does not wrap on reboot
    awk '{print int($1)}' /proc/uptime
}

wall_now_iso() {
    date -u +%Y-%m-%dT%H:%M:%SZ
}

get_boot_id() {
    cat /proc/sys/kernel/random/boot_id
}

get_mem_stats() {
    awk '/MemAvailable|MemFree|MemTotal/{printf "%s=%s ", $1, $2}' /proc/meminfo
}

emit_jsonl() {
    local state="$1"
    local extra="${2:-}"
    local uptime_secs=$(( $(mono_now_secs) - TRIAL_START_MONO ))
    local mem
    mem=$(get_mem_stats)

    printf '{"trial_id":"%s","boot_id":"%s","engine_pid":%s,"wall_time":"%s","monotonic_uptime_secs":%d,"mem_stats":"%s","trial_state":"%s","inv001_status":"UNCHECKED","watchdog_status":"OK"%s}\n' \
        "$TRIAL_ID" \
        "$BOOT_ID" \
        "${ENGINE_PID:--1}" \
        "$(wall_now_iso)" \
        "$uptime_secs" \
        "$mem" \
        "$state" \
        "${extra:+,$extra}" >> "$TELEMETRY_FILE"
}

fail_trial() {
    local reason="$1"
    TRIAL_STATE="TRIAL_FAILED"
    emit_jsonl "TRIAL_FAILED" "\"failure_reason\":\"$reason\""
    echo "[S5-REQ-003] TRIAL_FAILED: $reason" >&2
    # No restart. Exit non-zero to signal failure.
    exit 1
}

# â”€â”€â”€ Initialisation â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
mkdir -p "$TELEMETRY_DIR"
TELEMETRY_FILE="${TELEMETRY_DIR}/s5_trial_${TRIAL_ID}.jsonl"
# O_APPEND semantics enforced by shell >> redirection
touch "$TELEMETRY_FILE"

BOOT_ID=$(get_boot_id)
TRIAL_START_MONO=$(mono_now_secs)
INITIAL_BOOT_ID="$BOOT_ID"

# Target duration
if [[ "$DRY_RUN_SECS" -gt 0 ]]; then
    TARGET_SECS="$DRY_RUN_SECS"
    echo "[S5-REQ-003] DRY-RUN MODE: ${DRY_RUN_SECS}s (classification: INFRASTRUCTURE_VALIDATION)"
else
    TARGET_SECS="$TRIAL_DURATION_SECS"
    echo "[S5-REQ-003] TRIAL MODE: ${TRIAL_DURATION_HOURS}h (classification: INFRASTRUCTURE_VALIDATION until completion)"
fi

echo "[S5-REQ-003] Trial ID: ${TRIAL_ID}"
echo "[S5-REQ-003] Boot ID : ${BOOT_ID}"
echo "[S5-REQ-003] Telemetry: ${TELEMETRY_FILE}"
[[ -n "$ENGINE_PID" ]] && echo "[S5-REQ-003] Engine PID: ${ENGINE_PID}"

emit_jsonl "TRIAL_STARTED"
TRIAL_STATE="TRIAL_CONTINUOUS"
emit_jsonl "TRIAL_CONTINUOUS"

# â”€â”€â”€ Watchdog loop â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
LAST_INV001_CHECK=$(mono_now_secs)

while true; do
    sleep "$WATCHDOG_INTERVAL_SECS" &
    SLEEP_PID=$!
    wait $SLEEP_PID 2>/dev/null || true

    NOW=$(mono_now_secs)
    ELAPSED=$(( NOW - TRIAL_START_MONO ))

    # â”€â”€ Boot ID continuity check â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    CURRENT_BOOT_ID=$(get_boot_id)
    if [[ "$CURRENT_BOOT_ID" != "$INITIAL_BOOT_ID" ]]; then
        fail_trial "BOOT_ID_CHANGED from $INITIAL_BOOT_ID to $CURRENT_BOOT_ID"
    fi

    # â”€â”€ Engine PID continuity check â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if [[ -n "$ENGINE_PID" ]]; then
        if ! kill -0 "$ENGINE_PID" 2>/dev/null; then
            fail_trial "ENGINE_PID_DEAD pid=$ENGINE_PID elapsed=${ELAPSED}s"
        fi
    fi

    # â”€â”€ Monotonic clock sanity â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if [[ "$NOW" -lt "$TRIAL_START_MONO" ]]; then
        fail_trial "MONOTONIC_CLOCK_WENT_BACKWARD"
    fi

    # â”€â”€ INV-001 periodic check â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (( NOW - LAST_INV001_CHECK >= INV001_CHECK_INTERVAL_SECS )); then
        # INV-001: No WAL gaps. Check for any gap markers in Tier-0 if accessible.
        # This is a structural check; real WAL verification requires govcheck.
        INV001_STATUS="UNCHECKED"
        if command -v govcheck &>/dev/null; then
            if govcheck validate --scope P3 --strict 2>/dev/null; then
                INV001_STATUS="PASS"
            else
                fail_trial "INV001_VIOLATION detected by govcheck"
            fi
        fi
        LAST_INV001_CHECK="$NOW"
    fi

    # â”€â”€ Telemetry emit â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    emit_jsonl "TRIAL_CONTINUOUS"

    # â”€â”€ Completion check â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (( ELAPSED >= TARGET_SECS )); then
        break
    fi
done

# â”€â”€â”€ Completion â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
TRIAL_STATE="TRIAL_COMPLETED"
emit_jsonl "TRIAL_COMPLETED" '"evidence_class":"INFRASTRUCTURE_VALIDATION"'

# SHA-256 of the full telemetry artifact
DIGEST=$(sha256sum "$TELEMETRY_FILE" | awk '{print $1}')

echo "[S5-REQ-003] TRIAL_COMPLETED"
echo "[S5-REQ-003] Evidence class : INFRASTRUCTURE_VALIDATION"
echo "[S5-REQ-003] Telemetry file : ${TELEMETRY_FILE}"
echo "[S5-REQ-003] SHA-256        : ${DIGEST}"
echo ""
echo "[S5-REQ-003] NOTE: This script proves the trial state machine and runner"
echo "[S5-REQ-003] infrastructure only. It does NOT constitute LIVE_OPERATIONAL"
echo "[S5-REQ-003] evidence. A real 336-hour physical continuous run is required"
echo "[S5-REQ-003] to satisfy S5-REQ-003 for V4 promotion."
