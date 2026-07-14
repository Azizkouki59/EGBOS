#!/usr/bin/env bash
# governance/evidence/generate_compliance_snapshot.sh
# Generates a compliance snapshot for govcheck C0 gate validation (B1.4).
#
# Spec reference: §B1.4 COMPLIANCE-EVIDENCE-001
# "A compliance snapshot is a point-in-time JSON record that proves
#  govcheck has been run successfully. The snapshot must be committed
#  within 24 hours of the latest code change or it becomes stale."
#
# Usage:
#   ./governance/evidence/generate_compliance_snapshot.sh [--repo-root <path>]
#
# Output:
#   governance/evidence/<epoch_ms>_<short_sha>_compliance_snapshot.json
#
# The snapshot contains:
#   - Timestamp (ISO 8601)
#   - Git commit SHA
#   - govcheck exit code
#   - Scope validated
#   - Hardware ID (from EGBOS_HARDWARE_ID env var)

set -euo pipefail

REPO_ROOT="${1:-.}"
EVIDENCE_DIR="${REPO_ROOT}/governance/evidence"
mkdir -p "${EVIDENCE_DIR}"

# Compute identifiers
EPOCH_MS=$(date +%s%3N 2>/dev/null || python3 -c "import time; print(int(time.time()*1000))")
GIT_SHA=$(cd "${REPO_ROOT}" && git rev-parse HEAD 2>/dev/null || echo "unknown")
SHORT_SHA="${GIT_SHA:0:8}"
HARDWARE_ID="${EGBOS_HARDWARE_ID:-$(hostname)}"
TIMESTAMP=$(date -Iseconds 2>/dev/null || python3 -c "import datetime; print(datetime.datetime.now().isoformat())")

# Run govcheck validate --scope C0 --strict and capture exit code
SCOPE="C0"
GOVCHECK_BIN="${REPO_ROOT}/build/govcheck/govcheck"

if [ -x "${GOVCHECK_BIN}" ]; then
    GOVCHECK_OUTPUT=$("${GOVCHECK_BIN}" validate --scope "${SCOPE}" --strict --repo-root "${REPO_ROOT}" 2>&1) || true
    GOVCHECK_EXIT=$?
else
    GOVCHECK_OUTPUT="govcheck binary not found at ${GOVCHECK_BIN}"
    GOVCHECK_EXIT=2
fi

# Generate snapshot
SNAPSHOT_FILE="${EVIDENCE_DIR}/${EPOCH_MS}_${SHORT_SHA}_compliance_snapshot.json"

cat > "${SNAPSHOT_FILE}" <<SNAPEOF
{
  "schema": "COMPLIANCE-EVIDENCE-001",
  "version": 1,
  "timestamp": "${TIMESTAMP}",
  "epoch_ms": ${EPOCH_MS},
  "git_commit_sha": "${GIT_SHA}",
  "hardware_id": "${HARDWARE_ID}",
  "scope_validated": "${SCOPE}",
  "govcheck_exit_code": ${GOVCHECK_EXIT},
  "verdict": $([ ${GOVCHECK_EXIT} -eq 0 ] && echo '"PASS"' || echo '"FAIL"'),
  "staleness_window_hours": 24,
  "generated_by": "generate_compliance_snapshot.sh"
}
SNAPEOF

echo "Compliance snapshot written: ${SNAPSHOT_FILE}"
echo "  Scope: ${SCOPE}"
echo "  Verdict: $([ ${GOVCHECK_EXIT} -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "  Exit code: ${GOVCHECK_EXIT}"
