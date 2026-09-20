# S5 STEP 16 — Governance Board Decision Record

**Decision Reference:** EGBOS-GOV-DECISION-2663389-S5-16  
**Authority:** Governance Board  
**Date:** 2026-08-18T18:25:00Z  
**Preceding package:** `governance/amendments/S5_STEP15_GOVERNANCE_CORRECTION.md`  
**Status:** APPROVED

---

## Decision 1 — GOV-S5-15-A: REQ-001 HWM Acceptance Criterion

**Selection:** OPTION A

### Rationale

The existing acceptance criterion `HWM_upper_bound < 90% × 4096 = 3686 slots`
is preserved. It is architecturally sound and meaningful when measured under a
rate-calibrated jitter workload rather than an unlimited saturation workload.

The approved `PRODUCER_SIDE_UPPER_BOUND = head_ - tail_cache_` metric is
correct and cache-isolation compliant. The observed 4096/4096 result is a
harness design artifact, not a metric defect.

### Authorized Change

**File authorized for modification:** `scripts/s5_exec_ring_hwm_harness.cpp`

**Scope of authorized change — harness only:**

The harness producer thread must be amended to model a defined peak-exchange-jitter
workload as follows:

- Producer message rate: configurable inter-message interval, defaulting to
  **500 µs mean** with **±50 µs jitter** (uniform random) to simulate realistic
  exchange feed burst behavior.
- Consumer: unchanged — processes commands as fast as possible (models L5
  ExecutionRouter throughput).
- Duration: 60 seconds, unchanged.
- Ring, ring type, ring capacity: unchanged.
- Acceptance criterion: `HWM_upper_bound < 3686 slots` — unchanged.
- All govcheck compliance attributes (GC-E-010, GC-E-011, GC-E-042): unchanged.

**Prohibited changes:**

- No modification to `SpscRing<T, Capacity>` semantics.
- No cross-cacheline reads introduced.
- No `tail_cache_` refresh path changes.
- No `producer_hwm_upper_bound()` implementation changes.
- No `shm_layout.h`, `execution_router.h/cpp`, `wal_writer.h` modifications.

### Claim Amendment Authorized (conditional on passing harness)

After the amended harness produces a passing artifact (`HWM < 90%`):

The following fields of `CLAIM-L3-05.yaml` may be amended (and ONLY these):

| Field | Authorized new value |
|---|---|
| `class_rationale` | Must state: (a) metric is `PRODUCER_SIDE_UPPER_BOUND`, (b) measurement was rate-calibrated jitter scenario, (c) observed HWM value and percentage |
| `proof_artifacts` | Add entry for `governance/proof_artifacts/s5_req001_hwm_upper_bound.json` with actual computed SHA-256 |
| `rfc_references` | Add `EGBOS-GOV-DECISION-2663389-S5-16` |
| `amendments` | Add entry with this decision ID, description, and date |

**Fields that must NOT be changed:**

- `verification_class` — remains `V3` until LIVE_OPERATIONAL evidence is produced
- `composed_verification_class` — remains `V3`
- `minimum_class_for_unblocked_deploy` — remains `V3`
- `enforcement_gaps` — the live feed jitter gap entry must be preserved

---

## Decision 2 — GOV-S5-15-B: REQ-004 Encapsulation Authorization

**Selection:** APPROVED

### Rationale

The forensic audit (STEP 14) established that `GC-E-043` cannot be satisfied
by any external mechanism (`friend`, pointer casting, macro, include-trick).
The only govcheck-compliant serialization path requires the private-field
accesses to occur inside the owning class files, where the token-presence rule
does not apply.

The proposed `snapshot_into()` / `hydrate_from()` architecture:
- Does not weaken GC-E-043 or GC-E-044.
- Does not introduce cross-plane reads.
- Does not expose raw pointers or references to internal state.
- Does not modify any ABI files.
- Does not change SPSC ownership semantics.
- Is consistent with the ratified S5-REQ-004 serialization contract.

### Authorized File Scope

The following files are authorized for modification:

| File | Authorized change |
|---|---|
| `engine/core/position_engine.h` | Add `PositionSnapshot` struct definition; add `snapshot_into(const PositionSnapshot&) noexcept` and `hydrate_from(PositionSnapshot&) const noexcept` declarations; remove `friend class SnapshotSerializer/SnapshotDeserializer` |
| `engine/core/position_engine.cpp` | Add `snapshot_into()` and `hydrate_from()` definitions |
| `engine/core/risk_enforcer.h` | Add `RiskSnapshot` struct definition; add `snapshot_into()` and `hydrate_from()` declarations; remove `friend class SnapshotSerializer/SnapshotDeserializer` |
| `engine/core/risk_enforcer.cpp` | Add `snapshot_into()` and `hydrate_from()` definitions |
| `engine/l1/lob_engine.h` | Add `snapshot_into()` and `hydrate_from()` declarations; remove `friend class SnapshotSerializer/SnapshotDeserializer` |
| `engine/l1/lob_engine.cpp` | Add `snapshot_into()` and `hydrate_from()` definitions |
| `engine/core/snapshot_serializer.cpp` | Replace direct field access with `snapshot_into()` calls |
| `engine/core/snapshot_deserializer.cpp` | Replace direct field access with `hydrate_from()` calls |

### API Constraints (binding)

The following constraints from `S5_STEP15_GOVERNANCE_CORRECTION.md §3.3` are
binding on the implementation:

**PositionSnapshot:**
```
struct PositionSnapshot {
    int64_t  position;
    uint64_t order_tag;
    int64_t  x_l;
    int64_t  gross_exposure;
};
```
`snapshot_into()` must use `memory_order_acquire` on atomic fields.  
`hydrate_from()` must use `memory_order_release` on atomic fields.  
Both are `noexcept`. No heap allocation.

**RiskSnapshot:**
```
struct RiskSnapshot {
    uint32_t current_state;
    uint64_t last_heartbeat_tsc;
};
```
`QuotingState` cast to/from `uint32_t` by value.  
Both are `noexcept`. No heap allocation.

**LobEngine snapshot:**
- `snapshot_into(uint8_t* buf, size_t buf_len) const noexcept` — writes header + per-order records; returns bytes written, 0 on overflow.
- `hydrate_from(const uint8_t* buf, size_t len) noexcept` — returns `bool`.
- No `std::vector` parameter (GC-E-010 compliance).
- Deterministic order: ascending `price_index`, then ascending `order_id`.

**Prohibited:**
- No pointer or reference to internal engine state returned from any method.
- No dynamic allocation inside `snapshot_into()` or `hydrate_from()`.
- No modification to `shm_layout.h`, `wal_writer.h`, `execution_router.h/cpp`.
- No modification to the govcheck rule source.
- `position_engine.cpp` must be the only non-test file containing `order_tag_` or `x_l_` tokens after implementation.

### Verification Gate (required before STEP 16 implementation is considered complete)

After implementation:

1. `./build_s2/govcheck/govcheck validate --scope C0,P3,P4,P5 --strict` must exit 0.
2. `ninja test_snapshot_contract` must build and pass.
3. `git diff HEAD -- engine/core/shm_layout.h engine/core/wal_writer.h engine/core/execution_router.h engine/core/execution_router.cpp` must be empty.
4. `git diff HEAD -- governance/claims/CLAIM-L3-05.yaml` must be empty.

---

## Pre-Decision Repository State Verification

Performed immediately before recording this decision:

| Check | Result |
|---|---|
| `git diff HEAD -- governance/claims/CLAIM-L3-05.yaml` | Empty — byte-identical to HEAD ✓ |
| `git diff HEAD -- shm_layout.h, wal_writer.h, execution_router.{h,cpp}` | Empty — no changes ✓ |
| `git diff --check HEAD` (whitespace) | Fails on `CLAIM-L0-002.yaml` CRLF only (pre-existing, out-of-scope) ✓ |
| `govcheck validate --scope C0,P3,P4,P5 --strict` | 4 × GC-E-043 (snapshot_serializer.cpp:37,38 / snapshot_deserializer.cpp:57,58) — pre-existing, will be resolved by authorized implementation ✓ |
| `verification_class: CLAIM-L3-05` | `V3` — no claim promotion ✓ |
| Evidence promoted | None ✓ |
| Implementation source files changed in STEP 16 | None — this step is governance only ✓ |

---

## Authorized Implementation Scope Summary

| Item | Authorized file(s) | Decision reference |
|---|---|---|
| REQ-001 rate-calibrated harness | `scripts/s5_exec_ring_hwm_harness.cpp` | EGBOS-GOV-DECISION-2663389-S5-16 §Decision 1 |
| REQ-004 owner-side snapshot APIs | `engine/core/position_engine.{h,cpp}`, `engine/core/risk_enforcer.{h,cpp}`, `engine/l1/lob_engine.{h,cpp}`, `engine/core/snapshot_serializer.cpp`, `engine/core/snapshot_deserializer.cpp` | EGBOS-GOV-DECISION-2663389-S5-16 §Decision 2 |

---

**S5 STEP 16 — GOVERNANCE DECISION RECORDED — READY FOR AUTHORIZED IMPLEMENTATION**
