# GOVERNANCE BOARD DECISION — S5 STEP 33

Decision Authority: Governance Board
Decision Reference: EGBOS-GOV-DECISION-2663389-S5-32
Decision Timestamp: 2026-08-19T15:00:00Z

---

## G3 — PRODUCTION EXECUTABLE / BUILD IDENTITY

**GOV-S5-32-G3-A: A1 — REQUIRE_COMPLETE_BUILD_AND_EXECUTABLE_PROVENANCE**

Status: APPROVED

Every LIVE_OPERATIONAL REQ-004 replay artifact MUST record at minimum:
- Executable name/version
- Executable SHA-256
- Immutable source revision (git commit SHA)
- Submodule revisions (where applicable)
- Build-system revision
- Compiler/toolchain identity
- Linker/assembler identity (where applicable)
- Resolved dependency identity
- Build configuration and flags
- Target architecture and ABI
- CPU feature baseline
- OS/kernel/runtime identity
- Build-environment identity

**GOV-S5-32-G3-B: B2 — RECOMMENDED_ONLY**

Status: APPROVED

Independent reproducible build is RECOMMENDED but NOT REQUIRED.

---

## G4 — WAL ACQUISITION / RETENTION / INTEGRITY

**GOV-S5-32-G4-A: A3 — IMMUTABLE_OR_EQUIVALENT_AUDIT_TRAIL**

Status: APPROVED

Production Tier-0 WAL must be retained via immutable storage or an equivalent auditable chain-of-custody trail. The exact mechanism is not mandated; an equivalent audit trail satisfies this requirement.

**GOV-S5-32-G4-B: B1 — BOARD_MUST_DEFINE_NUMERIC_RETENTION_PERIOD**

Status: APPROVED — REQUIRES_FURTHER_GOVERNANCE

The Governance Board must supply the numeric WAL retention period in a subsequent explicit decision. No numeric retention period is authorized by this decision.

**GOV-S5-32-G4-C: C1 — REQUIRE_ENCRYPTION_AT_REST_AND_IN_TRANSIT**

Status: APPROVED

All production Tier-0 WAL material used for LIVE_OPERATIONAL evidence MUST be encrypted at rest and in transit.

---

## G5 — BYTE IDENTITY PROCEDURE

**GOV-S5-32-G5-A: A1 — RAW_BYTE_COMPARISON_REQUIRED**

Status: APPROVED

"Byte-identical" (per S5_REQ004_RATIFICATION.md §1) requires a complete raw byte comparison of the replay output against the live Tier-0 WAL log output. Hash-only comparison does not satisfy this requirement.

**GOV-S5-32-G5-B: B3 — BOTH**

Status: APPROVED

Raw byte comparison must be performed both on the Tier-0 host and on an isolated verifier.

---

## G6 — OPERATIONAL ARTIFACT SEMANTICS

**GOV-S5-32-G6-A: A1 — ADOPT_SEPARATE_HASH_SEMANTICS**

Status: APPROVED

The following distinct hash fields are adopted:

| Field | Meaning |
|---|---|
| `wal_segment_sha256` | SHA-256 of exact WAL segment bytes |
| `wal_manifest_sha256` | SHA-256 of ordered WAL segment manifest |
| `input_sha256` | SHA-256 of canonical replay-input manifest/bundle |
| `output_sha256` | SHA-256 of raw replay output artifact |
| `state_hash` | SHA-256 of canonical serialized snapshot state |
| `executable_sha256` | SHA-256 of exact executable bytes |
| `artifact_hash` | SHA-256 of complete evidence package/manifest |

**GOV-S5-32-G6-B: B1 — SIGNED_ATTESTATION**

Status: APPROVED

LIVE_OPERATIONAL evidence artifacts MUST carry a signed attestation in addition to cryptographic hashes.

**GOV-S5-32-G6-C: C1 — IMMUTABLE_OR_APPEND_ONLY_EVIDENCE_STORAGE**

Status: APPROVED

LIVE_OPERATIONAL evidence artifacts MUST be stored in immutable or append-only storage.

---

## G7 — FAILURE / CHAIN POLICY

**GOV-S5-32-G7-A: A1 — STRICT_RESTART_FROM_NIGHT_1**

Status: APPROVED

Any failed, incomplete, or disqualified night resets the 14-night counter to zero. The chain must restart from Night 1.

**GOV-S5-32-G7-B: B1 — ANY_CONTINUITY_FAILURE_INVALIDATES_SUCCESSOR_CHAIN**

Status: APPROVED

A continuity failure (`initial_state_hash(N+1) != final_state_hash(N)`) immediately invalidates all successor nights. No further replay may proceed until the chain is restarted from Night 1.

**GOV-S5-32-G7-C: C1 — PRODUCTION_HARDWARE_UNAVAILABLE => HALT / NON-QUALIFYING**

Status: APPROVED

If production Tier-0 hardware is unavailable, the night does not qualify. The run halts. The 14-night counter is NOT advanced.

---

## ADVISORY BOUNDARY

External standards and research referenced during S5 STEP 31/32 preparation are ADVISORY ONLY.

They are NOT independently binding EGBOS requirements unless explicitly incorporated by a Board decision above.

---

## IMPLEMENTATION AUTHORIZATION

| Item | Status |
|---|---|
| G3 | IMPLEMENTATION_AUTHORIZED (pending production source availability) |
| G4-A | IMPLEMENTATION_AUTHORIZED |
| G4-B | REQUIRES_FURTHER_GOVERNANCE (numeric retention period undefined) |
| G4-C | IMPLEMENTATION_AUTHORIZED |
| G5 | IMPLEMENTATION_AUTHORIZED (pending production source availability) |
| G6 | IMPLEMENTATION_AUTHORIZED |
| G7 | IMPLEMENTATION_AUTHORIZED |

---

## REQ-004 OPERATIONAL GATE

```text
LIVE_OPERATIONAL evidence: NONE
Production Tier-0 WAL: NOT AVAILABLE
Production Tier-0 hardware: NOT AVAILABLE
REQ-004: OPEN / BLOCKED
V4 promotion: DEFERRED
```

This governance decision does NOT constitute operational evidence. REQ-004 remains BLOCKED until legitimate LIVE_OPERATIONAL evidence is collected per the now-defined policy.
