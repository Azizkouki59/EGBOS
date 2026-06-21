# EGBOS S0 Exit Gate Checklist

This document tracks the completeness of Slice 0 (S0) bootstrap deliverables against the official specification.

## 1. Repository Structure & Toolchain
- [x] Repository directory structure matches spec §2 tree exactly.
- [x] `toolchain.lock` committed with required compiler and tool versions.
- [x] `rust-toolchain.toml` committed with pinned stable channel.
- [x] `pyproject.toml` committed with dev dependencies.
- [x] `.clang-format` and `.clang-tidy` configs committed at root.
- [x] `.gitignore` configured to exclude build outputs, rust target, py environments, and editor files.
- [x] `scripts/validate_toolchain.sh` committed and verified.

## 2. govcheck Enforcement Binary
- [x] `govcheck` and `govcheck_tests` compile cleanly.
- [x] Unit test suite compiles and runs cleanly (27 unit tests passing).
- [x] Sanitizer profiles (ASAN, TSAN, UBSAN) configured in CMakeLists.txt.
- [x] All 6 violation fixtures trigger expected error codes in `tests/governance/fixtures/verify_all_fixtures.sh`.
  - [x] `invalid_schema.yaml` -> `schema`
  - [x] `bad_artifact_sha.yaml` -> `GC-E-018`
  - [x] `bad_owner.yaml` -> `GC-E-019`
  - [x] `undeclared_ns_dep.yaml` -> `GC-E-023`
  - [x] `superseded_dep.yaml` -> `GC-E-025`
  - [x] `laundered_class.yaml` -> `GC-E-017`

## 3. Governance Assets
- [x] `governance/claims/CLAIM-000.yaml` (bootstrap claim) committed and active at V1.
- [x] `governance/claims/CLAIM-GOVCHECK-001.yaml` (self-verification claim) committed and active at V4.
- [x] `governance/team.yaml` (team membership registry) committed.
- [x] `governance/namespace-deps.yaml` (cross-namespace dependency registry) committed.
- [x] `governance/schemas/schema_version_registry.yaml` committed.
- [x] Self-test proof artifact generated under `governance/proof_artifacts/` and its SHA-256 updated in `CLAIM-GOVCHECK-001.yaml`.

## 4. CI/CD Infrastructure
- [x] C0 Gate: `.github/workflows/govcheck.yml` validates claims.
- [x] P1 Gate: `.github/workflows/format.yml` runs format, linting, and regression tests.
- [x] P2 Gate: `.github/workflows/build.yml` runs compiler build matrix under all sanitizer profiles.
- [x] Nightly workflow: `.github/workflows/sanitizers.yml` runs extended sanitizer suites.

## 5. ADR Deliverables
- [x] `schemas/common.fbs` (AD-009) committed with `schema_version` field.
- [x] `engine/core/shm_layout.h` (AD-006) committed with compile-time SHM constants placeholder.
- [x] `engine/CMakeLists.txt` committed.

## 6. S0 Completeness Verification
To verify the S0 exit gate command locally, run:
```bash
./build/govcheck/govcheck validate --scope C0 --strict --repo-root .
```
Expected output:
`summary.passed: true` with `0 errors` and `0 warnings`.
