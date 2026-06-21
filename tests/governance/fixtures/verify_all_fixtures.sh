#!/usr/bin/env bash
# tests/governance/fixtures/verify_all_fixtures.sh
# B1.4.3 — Verify each violation fixture triggers its expected error code.
set -euo pipefail

GOVCHECK="./build/govcheck/govcheck"
FIXTURES_DIR="govcheck/tests/fixtures"
ERRORS=0

verify_fixture() {
  local fixture="$1" expected_code="$2"
  local tmpdir=$(mktemp -d)
  mkdir -p "$tmpdir/governance/claims"
  cp "$FIXTURES_DIR/$fixture" "$tmpdir/governance/claims/"
  cp governance/team.yaml "$tmpdir/governance/"
  cp governance/namespace-deps.yaml "$tmpdir/governance/"

  output=$($GOVCHECK validate --scope C0 --repo-root "$tmpdir" 2>&1 || true)

  if echo "$output" | grep -q "$expected_code"; then
    echo "PASS: $fixture → $expected_code"
  else
    echo "FAIL: $fixture did not trigger $expected_code"
    ERRORS=$((ERRORS + 1))
  fi
  rm -rf "$tmpdir"
}

verify_fixture "invalid_schema.yaml"    "schema"
verify_fixture "bad_artifact_sha.yaml"  "GC-E-018"
verify_fixture "bad_owner.yaml"         "GC-E-019"
verify_fixture "undeclared_ns_dep.yaml" "GC-E-023"
verify_fixture "superseded_dep.yaml"    "GC-E-025"
verify_fixture "laundered_class.yaml"   "GC-E-017"

if [ "$ERRORS" -gt 0 ]; then
  echo "FIXTURE REGRESSION: $ERRORS fixture(s) failed"
  exit 1
fi
echo "ALL FIXTURES PASSED"
