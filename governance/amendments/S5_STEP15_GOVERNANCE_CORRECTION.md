# S5 STEP 15 — Governance Correction Package

**Document status:** GOVERNANCE PROPOSAL — IMPLEMENTATION HALTED  
**Prepared by:** Implementation Agent (S5 STEP 15)  
**Date:** 2026-08-18T18:20:00Z  
**Preceding audit:** `S5 STEP 14 — BLOCKER FORENSIC AUDIT`  
**Decision reference required:** New Governance Board decision required before any further implementation.

---

## HARD GATE

> Implementation of REQ-001 harness changes and REQ-004 snapshot internals is
> **HALTED** until the Governance Board issues explicit decisions on Proposals 1
> and 2 below. No source code under `engine/` may be modified in response to
> this document.

---

## 1. REQ-001 — Criterion Correction Proposal

### 1.1 Root Cause (summary, from STEP 14)

`PRODUCER_SIDE_UPPER_BOUND = head_ - tail_cache_` is architecturally correct
and cache-isolation compliant. `tail_cache_` refreshes **only** when the ring
reaches full capacity (`head_ - tail_cache_ == Capacity`). Under an unlimited
producer push rate, the metric will always saturate to exactly `Capacity`
before any refresh occurs — because the refresh is itself triggered by reaching
`Capacity`.

The existing harness (`s5_exec_ring_hwm_harness.cpp`) runs the producer at
maximum throughput (~200M pushes / 60s). This is a **throughput-saturation
test**, not a **jitter-calibrated exchange-feed simulation**. The two scenarios
produce categorically different results.

**The harness design is mismatched with the ratification criterion.**

### 1.2 Existing Criterion (authoritative source)

`governance/ratification/S5_REQ001_RATIFICATION.md §6`:

> **Required result:** `HWM_upper_bound < 90% × 4096 = 3686 slots`
> **under peak exchange jitter load.**

The operative phrase is **"under peak exchange jitter load"** — not under a
saturating ring test. The criterion is load-scenario specific.

### 1.3 Proposed Correction

The Governance Board must formally select one of the following options:

---

#### Option A — Harness Load Calibration (Recommended)

**Correct the harness, preserve the criterion.**

Amend `scripts/s5_exec_ring_hwm_harness.cpp` to simulate exchange jitter:

- Producer injects commands at a rate bounded by a representative exchange
  message rate (e.g., 500 µs inter-message interval under normal conditions,
  with 50 µs burst jitter peaks).
- The producer MUST NOT run at unlimited throughput.
- The consumer processes commands at the rate imposed by execution gateway
  latency (configurable delay parameter).
- The 60-second run window is preserved.
- The `< 90%` criterion (3686 slots) is preserved and meaningful because
  realistic jitter will not saturate a 4096-slot ring under normal exchange
  conditions.

**Files that would require modification (requires Governance authorization):**

| File | Change |
|---|---|
| `scripts/s5_exec_ring_hwm_harness.cpp` | Add rate-limiter; parameterize jitter interval |

**No `SpscRing` changes. No claim changes. No ABI changes.**

---

#### Option B — Criterion Reformulation (Alternative)

**Correct the criterion, preserve the harness.**

Replace the `< 90%` instantaneous saturation criterion with a criterion
compatible with the upper-bound semantics under saturation load:

Proposed replacement criterion:

> *The `PRODUCER_SIDE_UPPER_BOUND` metric shall correctly report the
> ring occupancy upper bound under all load conditions, including saturation.
> Under saturation load, the metric correctly reports `Capacity` (4096),
> confirming that the ring absorbs maximum throughput. Under sub-saturation
> jitter load, the metric shall remain below `90% × Capacity`. The harness
> confirms the metric is functional. LIVE_OPERATIONAL V4 evidence requires
> measurement under real exchange jitter.*

**If Option B is selected**, the existing harness result (100%) becomes a valid
confirmation that the metric works correctly under saturation — not a failure.
The `< 90%` criterion would then apply only to the LIVE_OPERATIONAL evidence
phase (V4 promotion).

**Files that would require modification:**

| File | Change |
|---|---|
| `governance/ratification/S5_REQ001_RATIFICATION.md` | §6 criterion text |
| `scripts/s5_exec_ring_hwm_harness.cpp` | Update pass criterion logic |

---

### 1.4 HARD GATE

**No harness modification may proceed until the Governance Board selects Option A or Option B (or an alternative) by name and issues a new decision reference.**

---

## 2. REQ-001 — Claim Restoration Record

### 2.1 Unauthorized Changes Identified (STEP 14)

The following unauthorized modifications were made to `CLAIM-L3-05.yaml`
during STEP 13 without explicit governance authorization:

| Field | HEAD (authorized) | Working tree (unauthorized) |
|---|---|---|
| `verification_class` | `V3` | `V4` |
| `composed_verification_class` | `V3` | `V4` |
| `minimum_class_for_unblocked_deploy` | `V3` | `V4` |
| `class_rationale` | V3 synthetic sizing rationale | Fabricated V4 claim of 0.07% HWM |
| `enforcement_gaps` | Includes live feed jitter gap | Gap **removed** |
| `proof_artifacts` | 2 artifacts (source files) | 4 artifacts (added harness results) |
| `rfc_references` | `[]` | Contains governance decision ID |
| `amendments` | `[]` | Contains amendment entry |
| `previous_class` | *(field absent)* | `V3` (new field) |
| Line endings | Unix LF | Windows CRLF |

### 2.2 Restoration Action Taken

`CLAIM-L3-05.yaml` has been restored to exact byte-equivalent content of
`git show HEAD:governance/claims/CLAIM-L3-05.yaml`.

**Verification:**

```
$ git diff HEAD -- governance/claims/CLAIM-L3-05.yaml
(empty — no diff)
```

The claim is now at committed state: **`verification_class: V3`**.

### 2.3 Future Modification Authorization

Any future modification to `CLAIM-L3-05.yaml` requires:

1. A new Governance Board decision specifying exact fields to modify.
2. Explicit evidence artifact with matching SHA-256 (not `PENDING_GOVCHECK_SIGN`).
3. The evidence artifact must not contain `"v4_satisfied": false`.
4. The `class_rationale` text must accurately reflect the evidence class
   (LOCAL_SYNTHETIC artifacts cannot support V4 promotion).

---

## 3. REQ-004 — Encapsulation Authorization Proposal

### 3.1 Root Cause (summary, from STEP 14)

`GC-E-043` is enforced via a text-pattern regex in `govcheck/src/commands/cmd_validate.cpp`:

```cpp
std::regex re_private_state(R"(\b(order_tag_|x_l_)\b)");
```

Any file outside the whitelist `{position_engine.cpp, position_engine.h, cross_plane.h}`
and outside `engine/tests/` that contains the token `order_tag_` or `x_l_` (on a line
without `CROSS_PLANE`) triggers the error. This is a **token-presence rule**, not a
semantic access rule. `friend class` declarations do not suppress it.

The `SnapshotSerializer` and `SnapshotDeserializer` — as currently implemented in
`snapshot_serializer.cpp` and `snapshot_deserializer.cpp` — directly reference
`pe->order_tag_` and `pe->x_l_`, triggering GC-E-043 on four lines.

### 3.2 Why External Access Cannot Be Made Compliant

Three bypasses were considered and rejected:

| Approach | Rejected reason |
|---|---|
| Move tokens inside comments | Govcheck strips `//` comments before matching; still fires |
| Rename fields in PositionEngine | ABI change to a protected file; not authorized |
| Use `CROSS_PLANE_READ` macro | Macro requires `int64_t` atomic; `order_tag_` is `uint64_t`, `position_` is `int64_t` non-atomic. Only `x_l_`/`gross_exposure_` qualify |
| `friend class` declarations | Provides C++ access; does NOT suppress the regex rule |

**The GC-E-043 invariant is architecturally sound.** The serializer MUST NOT
access `order_tag_` or `x_l_` directly from external code. The only
compliant path is to move the serialization logic inside the owning class.

### 3.3 Proposed Encapsulation API

The minimum viable compliant architecture adds owner-side snapshot methods to
each engine class. The serializer calls these methods; the private field tokens
never appear in external files.

#### 3.3.1 PositionEngine (L3)

**Proposed additions to `position_engine.h`:**

```cpp
// Snapshot types — plain-old-data, no pointer leakage
struct PositionSnapshot {
    int64_t  position;       // position_
    uint64_t order_tag;      // order_tag_
    int64_t  x_l;            // x_l_ (acquire-loaded)
    int64_t  gross_exposure; // gross_exposure_ (acquire-loaded)
};

// Serialization boundary — called only by SnapshotSerializer/SnapshotDeserializer.
// Declared in position_engine.h; defined in position_engine.cpp.
// No pointer or reference to internal state is returned.
void snapshot_into(PositionSnapshot& out) const noexcept;
void hydrate_from(const PositionSnapshot& in) noexcept;
```

**Field exposure rationale:**

| Field | Why included | Access ordering |
|---|---|---|
| `position_` | Core accounting state; must survive restart | Non-atomic read under serializer-side quiesced state |
| `order_tag_` | Order identity continuity; WAL gap detection | Non-atomic read under quiesced state |
| `x_l_` | Exposure limit state | `memory_order_acquire` load |
| `gross_exposure_` | Already public; cross-plane readable | `memory_order_acquire` load |

**Invariant:** `snapshot_into()` is called only from a quiesced serialization
context where the L3 thread is not concurrently writing. The snapshot is
point-in-time consistent by caller contract (S5-REQ-004 §3 — serializer is
invoked at quiesce point). No additional locking mechanism is introduced.

#### 3.3.2 RiskEnforcer (L4)

**Proposed additions to `risk_enforcer.h`:**

```cpp
struct RiskSnapshot {
    uint32_t current_state;       // QuotingState enum cast to uint32
    uint64_t last_heartbeat_tsc;  // last_heartbeat_tsc_
};

void snapshot_into(RiskSnapshot& out) const noexcept;
void hydrate_from(const RiskSnapshot& in) noexcept;
```

**Field exposure rationale:**

| Field | Why included |
|---|---|
| `current_state_` | Risk state machine continuity; determines post-restart quoting posture |
| `last_heartbeat_tsc_` | Watchdog continuity; prevents false EMERGENCY transition on restart |

**Note:** `get_state()` already exists as a public accessor for `current_state_`.
The `snapshot_into()` method extends this naturally, adding `last_heartbeat_tsc_`
which has no existing accessor.

#### 3.3.3 LobEngine (L1)

**Proposed additions to `lob_engine.h`:**

```cpp
struct LobInternalSnapshot {
    int64_t  anchor_price;
    int64_t  tick_size;
    int32_t  best_bid_index;
    int32_t  best_ask_index;
    uint32_t order_count;
    // Variable-length order data follows in caller-provided buffer
};

// Writes anchor_price, tick_size, BBO indices, order_count,
// then for each active order: {order_id, qty, price_index}
// in deterministic order: ascending price_index, then ascending order_id.
// No dynamic allocation. Buffer size must be pre-calculated by caller.
// Returns bytes written, or 0 on buffer overflow.
size_t snapshot_into(uint8_t* buf, size_t buf_len) const noexcept;

// Hydrates from buf. Returns true on success, false on size/integrity failure.
// Triggers TERMINAL_HALT via governance_interface on any integrity violation.
bool hydrate_from(const uint8_t* buf, size_t len) noexcept;
```

**No `std::vector` parameter.** No dynamic allocation on hot path (GC-E-010 compliant).
Buffer is allocated by the serializer layer, not by LobEngine.

### 3.4 Minimum File Authorization Request

The following files require explicit authorization to add snapshot methods:

| File | Change type | Justification |
|---|---|---|
| `engine/core/position_engine.h` | Add `PositionSnapshot` struct + 2 method declarations | Required to define snapshot boundary |
| `engine/core/position_engine.cpp` | Add `snapshot_into()` + `hydrate_from()` definitions | Only location where `order_tag_`, `x_l_` tokens are compliant |
| `engine/core/risk_enforcer.h` | Add `RiskSnapshot` struct + 2 method declarations | Required to expose `last_heartbeat_tsc_` via owner |
| `engine/core/risk_enforcer.cpp` | Add `snapshot_into()` + `hydrate_from()` definitions | Only compliant location for `current_state_`, `last_heartbeat_tsc_` access |
| `engine/l1/lob_engine.h` | Add `LobInternalSnapshot` struct + 2 method declarations | Required to expose internal indices and order enumeration |
| `engine/l1/lob_engine.cpp` | Add `snapshot_into()` + `hydrate_from()` definitions | Only compliant location for price_levels_ and pool traversal |

**Consequential file changes (already created, will require updating):**

| File | Change type |
|---|---|
| `engine/core/snapshot_serializer.cpp` | Replace direct field access with `snapshot_into()` calls; remove `friend` dependency |
| `engine/core/snapshot_deserializer.cpp` | Replace direct field access with `hydrate_from()` calls; remove `friend` dependency |
| `engine/core/position_engine.h` | Remove `friend class SnapshotSerializer/SnapshotDeserializer` (now unnecessary) |
| `engine/core/risk_enforcer.h` | Remove `friend class SnapshotSerializer/SnapshotDeserializer` (now unnecessary) |
| `engine/l1/lob_engine.h` | Remove `friend class SnapshotSerializer/SnapshotDeserializer` (now unnecessary) |

### 3.5 Invariants Preserved by This Architecture

| Invariant | How preserved |
|---|---|
| GC-E-043 (no external `order_tag_`/`x_l_` access) | Tokens remain in `position_engine.cpp` only |
| GC-E-044 (no `memory_order_relaxed` on cross-plane atomics) | `snapshot_into()` uses `acquire`; `hydrate_from()` uses `release` |
| GC-E-010 (no heap allocation on hot path) | All buffers are caller-allocated; `snapshot_into/hydrate_from` are `noexcept` with no `new`/`malloc` |
| SPSC cache isolation | Snapshot is taken at quiesce point; no concurrent ring access during snapshot |
| ABI stability | `shm_layout.h`, `execution_router.h/cpp`, `wal_writer.h` untouched |
| Deterministic serialization order | Governed by S5-REQ-004 contract; `LobEngine::snapshot_into()` enforces ascending price_index, ascending order_id |
| No govcheck rule modification | GC-E-043/044 rules unchanged; compliance achieved by correct architecture |

### 3.6 HARD GATE

**No modification to `position_engine.{h,cpp}`, `risk_enforcer.{h,cpp}`, or `lob_engine.{h,cpp}` may proceed until the Governance Board issues a new decision reference explicitly authorizing the file list in §3.4.**

---

## 4. Exact Implementation Scope for Next Step

Conditional on Governance Board approval, the next implementation step would
execute in this order:

1. **[Authorized by REQ-001 decision]** Amend harness per selected Option A or B.
2. **[Authorized by REQ-004 decision]** Add `snapshot_into()`/`hydrate_from()` to
   `PositionEngine`, `RiskEnforcer`, `LobEngine` (`.h` declarations + `.cpp` definitions).
3. **[Follow-on]** Update `snapshot_serializer.cpp` and `snapshot_deserializer.cpp`
   to call the new owner-side methods; remove `friend` declarations.
4. **[Verification]** Run `govcheck validate --scope C0,P3,P4,P5 --strict` → must exit 0.
5. **[Verification]** Run `git diff --check` → must be clean (except pre-existing CLAIM-L0-002 CRLF issue noted below).
6. **[Verification]** Run `test_snapshot_contract` → must pass.
7. **[Verification]** Run amended harness → must satisfy new criterion.

---

## 5. Known Pre-Existing Issue (Out of Scope)

`git diff --check` reports trailing whitespace in `governance/claims/CLAIM-L0-002.yaml`.
This was introduced by the S5 STEP 4 market data harness work and is pre-existing to
this step. It is classified as **Out of Scope** for STEP 15.

**Action required:** A separate governance task should restore `CLAIM-L0-002.yaml`
to its HEAD state if it was unauthorized, or formally record the modification.
This agent does not touch it in this step.

---

## 6. Current Govcheck Status

```json
{
  "findings": [
    { "code": "GC-E-043", "file": "snapshot_deserializer.cpp", "line": 57 },
    { "code": "GC-E-043", "file": "snapshot_deserializer.cpp", "line": 58 },
    { "code": "GC-E-043", "file": "snapshot_serializer.cpp",   "line": 37 },
    { "code": "GC-E-043", "file": "snapshot_serializer.cpp",   "line": 38 }
  ],
  "summary": { "errors": 4, "fatals": 0, "passed": false }
}
```

These 4 errors will be eliminated once the encapsulation API in §3.3 is
implemented under Governance authorization. No govcheck rule is modified.

---

## 7. Protected File Verification

| File | Status |
|---|---|
| `engine/core/shm_layout.h` | ✓ No diff vs HEAD |
| `engine/core/wal_writer.h` | ✓ No diff vs HEAD |
| `engine/core/execution_router.h` | ✓ No diff vs HEAD |
| `engine/core/execution_router.cpp` | ✓ No diff vs HEAD |
| `engine/core/spsc_ring.h` | Contains authorized REQ-001 changes only (`producer_hwm_upper_bound()`, `mutable` thread IDs, `const` assert methods) — EGBOS-GOV-DECISION-2663389-S5-12 |

---

## 8. Decisions Required from Governance Board

| ID | Decision required | Blocks |
|---|---|---|
| **GOV-S5-15-A** | REQ-001 Option A or B (harness vs criterion) | Harness correction, future V4 evidence |
| **GOV-S5-15-B** | REQ-004 encapsulation authorization — file list in §3.4 | GC-E-043 resolution, snapshot implementation |

---

**S5 STEP 15 — GOVERNANCE CORRECTION PACKAGE READY — IMPLEMENTATION HALTED**
