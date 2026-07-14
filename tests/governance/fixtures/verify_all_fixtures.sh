#!/usr/bin/env bash
# tests/governance/fixtures/verify_all_fixtures.sh
# B1.4.3 — Verify each violation fixture triggers its expected error code.
#
# Two categories of fixtures:
#   1. Claim-YAML fixtures (govcheck validate — §B1.4.3 claim schema)
#   2. Source-code violation fixtures (govcheck scan — §15.4 S1 governance)
#
# A passing violation fixture = rule fires = CI exit 0 for this script.
# A failing violation fixture = rule did NOT fire = CI exit 1.
set -euo pipefail

GOVCHECK="./build/govcheck/govcheck"
FIXTURES_DIR="govcheck/tests/fixtures"
S1_FIXTURES_DIR="tests/governance/fixtures"
ERRORS=0

# ── Category 1: Claim-YAML fixtures (govcheck validate) ──────────────────────

verify_claim_fixture() {
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

verify_claim_fixture "invalid_schema.yaml"    "schema"
verify_claim_fixture "bad_artifact_sha.yaml"  "GC-E-018"
verify_claim_fixture "bad_owner.yaml"         "GC-E-019"
verify_claim_fixture "undeclared_ns_dep.yaml" "GC-E-023"
verify_claim_fixture "superseded_dep.yaml"    "GC-E-025"
verify_claim_fixture "laundered_class.yaml"   "GC-E-017"

# ── Category 2: Source-code violation fixtures (govcheck scan — §15.4) ────────
# These fixtures are intentionally non-compliant source files.
# govcheck scan must detect the violation pattern and report the rule ID.

verify_source_fixture() {
  local fixture="$1" expected_code="$2"

  # govcheck scan examines the file for source-level rule violations
  output=$($GOVCHECK scan --file "$S1_FIXTURES_DIR/$fixture" 2>&1 || true)

  if echo "$output" | grep -q "$expected_code"; then
    echo "PASS: $fixture → $expected_code"
  else
    echo "FAIL: $fixture did not trigger $expected_code"
    ERRORS=$((ERRORS + 1))
  fi
}

# §15.4 — All 9 violation fixtures
verify_source_fixture "violation_GC_E_010.cpp"         "GC-E-010"
verify_source_fixture "violation_GC_E_011.cpp"         "GC-E-011"
verify_source_fixture "violation_GC_WAL_001.cpp"       "GC-WAL-001"
verify_source_fixture "violation_GC_E_053.cpp"         "GC-E-053"
verify_source_fixture "violation_GC_E_054.cpp"         "GC-E-054"
verify_source_fixture "violation_GC_E_SECRETS_001.sh"  "GC-E-SECRETS-001"
verify_source_fixture "violation_GC_E_056.js"          "GC-E-056"
verify_source_fixture "violation_GC_E_044.cpp"         "GC-E-044"
verify_source_fixture "violation_GC_E_045.cpp"         "GC-E-045"

# ── GC-E-053 companion claim YAML fixture ────────────────────────────────────
# The GC-E-053 rule fires on claim YAML, not source code.
verify_claim_fixture_from_s1() {
  local fixture="$1" expected_code="$2"
  local tmpdir=$(mktemp -d)
  mkdir -p "$tmpdir/governance/claims"
  cp "$S1_FIXTURES_DIR/$fixture" "$tmpdir/governance/claims/"
  cp governance/team.yaml "$tmpdir/governance/"
  cp governance/namespace-deps.yaml "$tmpdir/governance/"

  output=$($GOVCHECK validate --scope C0 --repo-root "$tmpdir" 2>&1 || true)

  if echo "$output" | grep -q "$expected_code"; then
    echo "PASS: $fixture → $expected_code (claim-level)"
  else
    echo "FAIL: $fixture did not trigger $expected_code (claim-level)"
    ERRORS=$((ERRORS + 1))
  fi
  rm -rf "$tmpdir"
}

verify_claim_fixture_from_s1 "fixture_GC_E_053_claim.yaml" "GC-E-053"

# ── Summary ───────────────────────────────────────────────────────────────────

if [ "$ERRORS" -gt 0 ]; then
  echo "FIXTURE REGRESSION: $ERRORS fixture(s) failed"
  exit 1
fi
echo "ALL FIXTURES PASSED"
