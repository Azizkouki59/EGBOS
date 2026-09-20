# S5 STEP 32 — GOVERNANCE BOARD DECISION PACKAGE

## Governance Authority
* `EGBOS-GOV-DECISION-2663389-S5-29`
* `S5_REQ004_RATIFICATION.md`
* `S5_REQ004_SNAPSHOT_AMENDMENT.md`
* `s5_artifact_schema.yaml`
* `S5 STEP 31 GOVERNANCE REVIEW` findings

## Current State
```text
REQ-004: OPEN / BLOCKED
LIVE_OPERATIONAL evidence: NONE
V4 promotion: DEFERRED
CLAIM-L3-05: V3 / unchanged
```

---

## G3 — PRODUCTION EXECUTABLE / BUILD IDENTITY

For every LIVE_OPERATIONAL REQ-004 replay run, the following provenance fields are proposed to be recorded:
* Executable name/version
* Executable SHA-256
* Immutable source revision (git commit SHA)
* Submodule revisions (where applicable)
* Build-system revision
* Compiler/toolchain identity
* Linker/assembler identity (where applicable)
* Resolved dependency identity
* Build configuration and flags
* Target architecture and ABI
* CPU feature baseline
* OS/kernel/runtime identity
* Build-environment identity

### Board Decisions Required

```text
GOV-S5-32-G3-A:
  A1 = REQUIRE_COMPLETE_BUILD_AND_EXECUTABLE_PROVENANCE
  A2 = REQUIRE_EXECUTABLE_SHA256_AND_SOURCE_REVISION_ONLY
```

```text
GOV-S5-32-G3-B:
  B1 = REQUIRE_INDEPENDENT_REPRODUCIBLE_BUILD
  B2 = RECOMMENDED_ONLY
  B3 = NOT_REQUIRED
```

---

## G4 — WAL ACQUISITION / RETENTION / INTEGRITY

Production Tier-0 WAL evidence is proposed to require identification of:
* Authoritative WAL source identity (host/service)
* Database/cluster identity (where applicable)
* Timeline/branch identity
* Exact segment range (start/end positions)
* Ordered segment list with per-segment size
* Per-segment SHA-256
* Manifest SHA-256
* Acquisition timestamp
* Source/destination verification
* Access identity and custody events

Protected immutable evidence storage is proposed for all acquired WAL material.

### Board Decisions Required

```text
GOV-S5-32-G4-A:
  A1 = POLICY_GOVERNED_RETENTION
  A2 = IMMUTABLE_RETENTION_REQUIRED
  A3 = IMMUTABLE_OR_EQUIVALENT_AUDIT_TRAIL
```

```text
GOV-S5-32-G4-B:
  B1 = BOARD_MUST_DEFINE_NUMERIC_RETENTION_PERIOD
  B2 = USE_EXISTING_ORGANIZATIONAL_RETENTION_POLICY
```

```text
GOV-S5-32-G4-C:
  C1 = REQUIRE_ENCRYPTION_AT_REST_AND_IN_TRANSIT
  C2 = FOLLOW_EXISTING_SECURITY_POLICY
```

> No numeric retention period is proposed here. A numeric value requires explicit Board authorization.

---

## G5 — BYTE IDENTITY PROCEDURE

The phrase "byte-identical against the Tier-0 WAL log" (S5_REQ004_RATIFICATION.md §1) directly implies raw byte comparison.

Proposed verification procedure:
1. Verify completeness of both artifacts (size > 0).
2. Record artifact sizes.
3. Perform complete raw byte comparison of both artifacts.
4. Calculate SHA-256 of both artifacts independently.
5. Calculate comparison-manifest SHA-256.
6. Record comparison tool name and version.
7. Record invocation command and options verbatim.
8. Record exit status.
9. Preserve full diagnostic output.
10. Classify any size mismatch or any byte mismatch as failure.

> A1 (raw byte comparison) is the direct implementation of "byte-identical" but is presented as a proposal only. The Board must decide.

### Board Decisions Required

```text
GOV-S5-32-G5-A:
  A1 = RAW_BYTE_COMPARISON_REQUIRED
  A2 = HASH_ONLY_COMPARISON
```

```text
GOV-S5-32-G5-B:
  B1 = COMPARISON_ON_TIER0_HOST
  B2 = COMPARISON_ON_ISOLATED_VERIFIER
  B3 = BOTH
```

---

## G6 — OPERATIONAL ARTIFACT SEMANTICS

S5 STEP 31 identified ambiguity in `input_sha256` / `output_sha256` in `s5_artifact_schema.yaml`. The following proposed semantic disambiguations are presented for Board review:

| Field | Proposed Meaning |
|---|---|
| `wal_segment_sha256` | SHA-256 of the exact WAL segment bytes |
| `wal_manifest_sha256` | SHA-256 of the ordered WAL segment manifest |
| `input_sha256` | SHA-256 of the canonical replay-input manifest/bundle |
| `output_sha256` | SHA-256 of the raw replay output artifact |
| `state_hash` | SHA-256 of the canonical serialized snapshot state |
| `executable_sha256` | SHA-256 of the exact executable bytes |
| `artifact_hash` | SHA-256 of the complete evidence package/manifest |

### Board Decisions Required

```text
GOV-S5-32-G6-A:
  A1 = ADOPT_SEPARATE_HASH_SEMANTICS
  A2 = RETAIN_CURRENT_SCHEMA_SEMANTICS_PENDING_FURTHER_REVIEW
```

```text
GOV-S5-32-G6-B:
  B1 = SIGNED_ATTESTATION
  B2 = CRYPTOGRAPHIC_HASH_ONLY
  B3 = EXISTING_GOVERNANCE_INTEGRITY_MODEL
```

```text
GOV-S5-32-G6-C:
  C1 = IMMUTABLE_OR_APPEND_ONLY_EVIDENCE_STORAGE
  C2 = EXISTING_STORAGE_CONTROLS
```

---

## G7 — FAILURE / CHAIN POLICY

### Proposed Night-State Model

```text
NOT_STARTED
INPUT_VALIDATION
READY
RUNNING
NIGHT_PASS
READY_FOR_NEXT_NIGHT
QUALIFIED_14
FAIL
HALT
QUARANTINED
```

### Proposed Night Qualification Rule

A night counts as qualifying ONLY when ALL of the following conditions hold:
* Input state is valid and accepted
* WAL is valid and integrity-verified
* Executable identity is accepted per G3 policy
* Tier-0 hardware identity is confirmed per G2 policy
* Replay succeeds without error
* Output is complete (no truncation)
* Byte comparison succeeds per G5 policy
* All required hashes are recorded
* Final state hash is recorded
* Continuity invariant `initial_state_hash(N+1) == final_state_hash(N)` holds
* Evidence artifact is complete and schema-valid
* No disqualifying limitation exists

A failed, incomplete, or unverifiable night must NOT count toward the 14-night threshold.

### Board Decisions Required

```text
GOV-S5-32-G7-A:
  A1 = STRICT_RESTART_FROM_NIGHT_1
  A2 = CONTROLLED_RETRY
  A3 = PAUSE_AND_RESUME
```

```text
GOV-S5-32-G7-B:
  B1 = ANY_CONTINUITY_FAILURE_INVALIDATES_SUCCESSOR_CHAIN
  B2 = ALLOW_CONTROLLED_RECOVERY_WITH_EXPLICIT_REVIEW
```

```text
GOV-S5-32-G7-C:
  C1 = PRODUCTION_HARDWARE_UNAVAILABLE => HALT / NON-QUALIFYING
  C2 = ALLOW_APPROVED_EQUIVALENT_HARDWARE
```

---

## EXTERNAL RESEARCH STATUS

External standards and guidance referenced during S5 STEP 31/32 preparation are ADVISORY ONLY.

They provide technical rationale for:
* Provenance and reproducibility requirements
* Cryptographic integrity mechanisms
* Evidence preservation approaches
* Controlled WAL acquisition procedures
* Raw byte comparison methodology
* Auditability frameworks

They do NOT independently establish EGBOS-specific:
* Numeric WAL retention periods
* 14-night restart rules
* Exact comparison host requirements
* Mandatory WORM storage
* Mandatory signed attestations
* Mandatory reproducible builds

Those remain Governance Board decisions.

---

## REQUIRED BOARD INPUT

```text
GOV-S5-32-G3-A: <A1 or A2>
GOV-S5-32-G3-B: <B1, B2 or B3>

GOV-S5-32-G4-A: <A1, A2 or A3>
GOV-S5-32-G4-B: <B1 or B2>
GOV-S5-32-G4-C: <C1 or C2>

GOV-S5-32-G5-A: <A1 or A2>
GOV-S5-32-G5-B: <B1, B2 or B3>

GOV-S5-32-G6-A: <A1 or A2>
GOV-S5-32-G6-B: <B1, B2 or B3>
GOV-S5-32-G6-C: <C1 or C2>

GOV-S5-32-G7-A: <A1, A2 or A3>
GOV-S5-32-G7-B: <B1 or B2>
GOV-S5-32-G7-C: <C1 or C2>

Decision Authority: Governance Board
Decision Reference: <BOARD INPUT REQUIRED>
Decision Timestamp: <BOARD INPUT REQUIRED>
```
