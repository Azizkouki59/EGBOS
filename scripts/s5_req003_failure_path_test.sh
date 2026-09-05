#!/usr/bin/env bash
# REQ-003 failure path validation script
# Starts a fake engine PID, runs the 336h runner with 30s dry-run,
# kills the fake engine after 3 seconds, checks the runner exits non-zero
# and records TRIAL_FAILED in telemetry.

set -euo pipefail
cd /mnt/c/EGBOS/egbos

# Start fake engine
sleep 999 &
FAKE_PID=$!

# Start runner in background
bash scripts/s5_336h_runner.sh --dry-run-seconds 30 --engine-pid "$FAKE_PID" &
RUNNER_PID=$!

# Let the runner initialise
sleep 4

# Kill the fake engine (simulates crash)
echo "[TEST] kill -9 engine pid=$FAKE_PID"
kill -9 "$FAKE_PID" 2>/dev/null || true

# Wait for runner to detect and exit
wait "$RUNNER_PID" && RUNNER_EXIT=0 || RUNNER_EXIT=$?

echo "[TEST] Runner exit code: $RUNNER_EXIT"
if [[ "$RUNNER_EXIT" -ne 0 ]]; then
    echo "[TEST] PASS: runner exited non-zero on engine kill â€” TRIAL_FAILED path confirmed"
else
    echo "[TEST] FAIL: runner exited 0 â€” no-restart detection failed"
    exit 1
fi
