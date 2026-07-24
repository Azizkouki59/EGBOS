# EGBOS — S3 Institutional Implementation Specification
## Position Engine (L3) + L4 Risk Enforcer

---

| Field | Value |
|---|---|
| **Document ID** | EGBOS-SPEC-S3-001 |
| **Classification** | Implementation Authority Document — Tier 0 |
| **Status** | ACTIVE — Authoritative for S3 |
| **Owner** | Chief Architecture Office |
| **Approval Authority** | CTO (Founder) |
| **Source of Truth** | `docs/EGBOS_MASTER.md` (converted from `EGBOS_MASTER_augmented.html` Freeze Authority Edition v1.0) |
| **Governing Principle** | P-004 (Governance Precedes Implementation) |
| **Supersedes** | Any prior informal S3 description in chat, notes, or generated documents |
| **Conflict Rule** | If any statement in this document conflicts with `docs/EGBOS_MASTER.md`, the Master prevails unconditionally |
| **Entry Dependency** | S2 acceptance gate must be signed. CLAIM-LOB-001 at V4. |

---

## Table of Contents

1. [Objectives and Scope](#1-objectives-and-scope)
2. [Architectural Overview](#2-architectural-overview)
3. [Repository Structure](#3-repository-structure)
4. [Position Engine Architecture (L3)](#4-position-engine-architecture-l3)
5. [Operational Loop (E2)](#5-operational-loop-e2)
6. [Pre-flight Protocol (E3)](#6-pre-flight-protocol-e3)
7. [Risk Governance Framework (L4)](#7-risk-governance-framework-l4)
8. [Exchange Connectivity Governance (L4C)](#8-exchange-connectivity-governance-l4c)
9. [Trading Audit Framework (L4E)](#9-trading-audit-framework-l4e)
10. [Shared Memory Interface Contracts](#10-shared-memory-interface-contracts)
11. [Govcheck Rules Active in S3](#11-govcheck-rules-active-in-s3)
12. [Claim Registry for S3](#12-claim-registry-for-s3)
13. [CI Gate Requirements](#13-ci-gate-requirements)
14. [ADR Dependencies](#14-adr-dependencies)
15. [Forbidden Actions](#15-forbidden-actions)
16. [Invariants](#16-invariants)
17. [Interfaces](#17-interfaces)
18. [Test Requirements](#18-test-requirements)
19. [Verification Classes](#19-verification-classes)
20. [Exit Criteria (Acceptance Gate)](#20-exit-criteria-acceptance-gate)
21. [Entry Conditions](#21-entry-conditions)
22. [Failure Mode Specifications](#22-failure-mode-specifications)
23. [Exit Gates for S3](#23-exit-gates-for-s3)

---

## 1. Objectives and Scope

### 1.1 Slice Identity

**S3 — Position Engine (L3) + L4 Risk Enforcer** is the fourth implementation slice in the Execution Plane registry (S0–S7), mapped to ENGINE_V3 Phase 3 and A2.2 Operating Phase I (Infrastructure Qualification). It carries **no capital gate**; no real or staging capital may be deployed as a consequence of S3 completion alone. Staging capital access requires G1 gate passage, which is the G-gate activated at S3 exit.

S3 is the first slice in which the execution engine's position correctness invariants are proved under real crash conditions. Every claim proved in S3 that is carried forward is the direct dependency for all live-capital gates (G1 → G2 → LIVE-CAPITAL-001). No S4 work may open until the S3 acceptance gate passes. The parallel Research Platform window (S8–S15) opens only at S3-exit.

### 1.2 Objectives

S3 must deliver, at the required verification class, two tightly coupled subsystems:

1. **Position Engine (L3)** — the in-process state machine that owns all position-affecting state. It is the single-writer for `egbos_position_state`. Its correctness is provable by invariant: `position = Σ FILL_APPLIED` at any point in WAL history. It enforces WAL-before-state ordering on every fill, mass cancel, and position sync event. It is the enforcement boundary for GC-E-043 (state ownership), GC-E-044 (memory ordering), and GC-E-045 (cross-plane read isolation).

2. **L4 Risk Enforcer** — the out-of-process C++ daemon that monitors `gross_exposure_` via the `CROSS_PLANE_READ` macro from shared memory, reads Registry entry `REG-RISK-001`, writes monotone latches to `/dev/shm/egbos_risk_limits`, and enforces the hardware kill switch (`CLAIM-L4-001`). L4 operates completely decoupled from L3 — it may not call into L3, acquire its locks, or write to the WAL.

### 1.3 Scope

**In scope for S3:**

- `engine/core/position_engine.h` and its implementation: the L3 position state machine
- `engine/core/risk_enforcer.h` and its implementation: the out-of-process L4 risk daemon
- `engine/core/governance_interface.h`: `GovernanceFlags` SHM structure (`halt_required`, `feed_stale`, `preflight_armed`, `pos_limit_latch`)
- `engine/core/cross_plane.h`: the `CROSS_PLANE_READ` macro (already defined in S1; used in S3)
- L4A hard/soft limit allocation and enforcement logic
- L4B quoting state machine (`INACTIVE → QUOTING → HEDGING → TAKER_REDUCE → EMERGENCY`)
- L4C hardware kill switch watchdog and heartbeat emission
- WAL record types `FILL_APPLIED`, `MASS_CANCEL`, `POSITION_SYNC` (complement S1's `ORDER_NEW`)
- Pre-flight checks PF-01 through PF-12 (full implementation required; see §6)
- Fixture tests for all GC-E rules activated in S3
- CLAIM-RISK-STALE-001 enforcement (`REG-RISK-STALE-001`; stale threshold ≤ 5000ms)

**Explicitly out of scope for S3:**

- LOB engine (L1) — this is S2 (complete)
- Execution Router (L5) — this is S4
- Live FIX/OUCH exchange connectivity — this is S5
- Production hardware timing measurement (`timing_constants.json` empirical) — this is S6
- Live capital deployment infrastructure — this is S7
- AMD Alveo U50 FPGA firmware authoring — hardware engineering workstream, not S3 software gate
- Python VaR/CVaR engine implementation — this is Research Platform (S8+)

---

## 2. Architectural Overview

S3 builds the state management and safety enforcement layer above the S1/S2 foundation:

```
LAYER 1: EXECUTION PLANE — L3 (S3 scope)
┌──────────────────────────────────────────────────────────────────────────┐
│  [S3] Position Engine (L3)                                               │
│       - Single writer: egbos_position_state                              │
│       - WAL record types: FILL_APPLIED, MASS_CANCEL, POSITION_SYNC      │
│       - Invariant: position = Σ FILL_APPLIED at any WAL checkpoint       │
│       - State ownership: order_tag_, x_l_ private; no public getters     │
│       - CROSS_PLANE_READ: gross_exposure_ sole cross-plane atomic        │
└──────────────────────────────────────────────────────────────────────────┘
                         ↕ SHM: egbos_position_state (CROSS_PLANE_READ)
┌──────────────────────────────────────────────────────────────────────────┐
│  [S3] L4 Risk Enforcer (out-of-process)                                  │
│       - Reads: gross_exposure_ via CROSS_PLANE_READ macro only           │
│       - Writes: egbos_risk_limits (monotone latches, AD-010)             │
│       - Kill switch: < 1ms response (CLAIM-L4-001, TSAN-verified)        │
│       - Soft limits: Redis Streams from Python VaR engine (AD-008)       │
│         Stale threshold: ≤ 5s (REG-RISK-STALE-001, GC-E-050)            │
│       - State machine: INACTIVE→QUOTING→HEDGING→TAKER_REDUCE→EMERGENCY  │
└──────────────────────────────────────────────────────────────────────────┘
                         ↕ SHM: egbos_governance_flags
┌──────────────────────────────────────────────────────────────────────────┐
│  [S1/S2 — complete]  WAL │ SPSC Rings │ SHM Layout │ LOB Engine (L1)    │
└──────────────────────────────────────────────────────────────────────────┘
```

**Central architectural thesis governing S3** (Master §E2.1):

> *The WAL is the definition of what happened. Position is not a mutable database field — it is the deterministic projection of the FILL_APPLIED event stream. If a FILL_APPLIED record is not in the WAL, that fill did not happen from the perspective of the governance system.*

**L4 architectural thesis** (Master §K4.1):

> *Institutional risk controls must operate independently of the execution engine thread to ensure safety in the event of software crashes or deadlocks. The out-of-process risk enforcer must be able to detect a missed heartbeat and enforce the kill switch even when L3 is blocked, deadlocked, or in TERMINAL_HALT.*

All cross-plane read rules (GC-E-043, GC-E-044, GC-E-045) and all monotone latch requirements (AD-010) must be structurally enforced by govcheck — not by convention. P-001 (Mechanical Enforcement Supremacy) applies fully.

---

## 3. Repository Structure

S3 artifacts extend the directory structure established in S0–S2. The following paths are authoritative (Master §C3, §E1, §B1.8):

```
engine/
  core/
    position_engine.h          # L3 position state machine — single writer
    position_engine.cpp        # L3 implementation
    risk_enforcer.h            # L4 out-of-process risk daemon interface
    risk_enforcer.cpp          # L4 implementation
    governance_interface.h     # GovernanceFlags SHM struct (exists S1; extended S3)
    cross_plane.h              # CROSS_PLANE_READ macro (exists S1; used S3)
    shm_layout.h               # constexpr SHM region sizes — extended for L3/L4 regions
    wal_writer.h               # (S1, unchanged) — FILL_APPLIED record type added

governance/
  claims/
    CLAIM-L3-001.yaml          # exec.wal_ordering — target V4
    CLAIM-INV-001.yaml         # exec.position — target V4
    CLAIM-L3-003.yaml          # exec.state_ownership — target V3
    CLAIM-L3-004.yaml          # exec.memory_ordering — target V4
    CLAIM-L3-006.yaml          # exec.atomic_exposure — target V4
    CLAIM-L3-MAR-001.yaml      # exec.mass_cancel — target V4
    CLAIM-L4-001.yaml          # risk.kill_switch — target V4
    CLAIM-RISK-STALE-001.yaml  # risk.soft_limit_stale — target V1
  proof_artifacts/
    position_kill9_results/    # 10× kill-9 position recovery records (V4 evidence)
    l4_tsan_results/           # L4 kill switch TSAN run log (V4 evidence)
    10m_fill_results/          # 10M fill invariant run (V4 evidence for CLAIM-INV-001)
  registry/
    REG-RISK-STALE-001.yaml    # Soft limit stale threshold — production value ≤ 5000ms

tests/
  governance/
    fixtures/
      violation_GC_E_043.cpp   # Direct order_tag_ access outside L3 — must fire GC-E-043
      violation_GC_E_044.cpp   # memory_order_relaxed on cross-plane atomic — fires GC-E-044
      violation_GC_E_045.cpp   # Direct x_l_ read from L4/L5 — must fire GC-E-045
  unit/
    position_engine_test.cpp   # L3 unit test suite (WAL-before-state, INV-001, MAR-001)
    risk_enforcer_test.cpp     # L4 unit test + kill switch TSAN test

scripts/
  preflight.sh                 # Pre-flight arming sequence (PF-01 through PF-12)
```

**Namespace conventions (Master §B1.8):**
- `exec.*` — Position engine and execution correctness claims
- `risk.*` — Risk enforcer claims and limits
- `wal.*` — WAL ordering (inherited from S1)

All claim files must use `schema_version: 1`. All `owner` fields must reference a GitHub username present in `governance/team.yaml`.

---

## 4. Position Engine Architecture (L3)

*Authoritative source: Master §E1, §E2, §C2, §B3*

### 4.1 L3 Architectural Thesis

The position engine is the sole authority on position state in the EGBOS runtime. It is a **single-writer** to `/dev/shm/egbos_position_state`. No other module may write to this region. No other module may read `order_tag_` or `x_l_` fields directly — these are private, enforced by GC-E-043 and GC-E-045.

The position invariant is constitutional (Master §B3, CLAIM-INV-001):

> **INV-001:** At any point in WAL history, `reconstructed_position == Σ FILL_APPLIED`. Any delta is a P0 incident and triggers TERMINAL_HALT.

### 4.2 WAL Record Types for S3

S3 introduces three new WAL record types that complement S1's `ORDER_NEW`:

| Record Type | Trigger | WAL-before-state requirement |
|---|---|---|
| `FILL_APPLIED` | Exchange fill received | WAL written BEFORE position incremented |
| `MASS_CANCEL` | MassActionReport received (L4 triggered or exchange-initiated) | WAL written BEFORE any position state updated (CLAIM-L3-MAR-001) |
| `POSITION_SYNC` | Pre-flight PF-02 reconstruction checkpoint | WAL written at session boundary |

These extend the existing S1 `WalRecord` struct. The `record_type` enum must be extended in `wal_writer.h` with `FILL_APPLIED`, `MASS_CANCEL`, and `POSITION_SYNC` values. No schema change to the 64-byte header layout is permitted without a new ADR.

### 4.3 State Ownership Contract

The L3 position engine owns the following in-memory fields **exclusively**:

| Field | Type | Access Policy |
|---|---|---|
| `position_` | `int64_t` (fixed-point, AD-004) | L3 private, no getter |
| `order_tag_` | `uint64_t` | L3 private; GC-E-043 blocks any external read |
| `x_l_` | `std::atomic<int64_t>` | L3 private; GC-E-045 blocks any L4/L5 read |
| `gross_exposure_` | `std::atomic<int64_t>` | CROSS_PLANE_READ only; GC-E-044 governs memory order |

**CROSS_PLANE_READ protocol** (Master §E1.3, CLAIM-L3-004, CLAIM-L3-006):

- `gross_exposure_` is the **sole** field readable cross-plane by L4.
- All reads of `gross_exposure_` by L4 must use the `CROSS_PLANE_READ(gross_exposure_)` macro.
- `CROSS_PLANE_READ` documents the memory order at the access site and generates a TSAN annotation.
- `memory_order_relaxed` is **forbidden** on any cross-plane atomic (GC-E-044 blocks at P4).

```cpp
// engine/core/cross_plane.h — canonical usage
// L4 risk enforcer reads gross_exposure_ via this macro ONLY
// Any direct field access is GC-E-045 violation
int64_t exposure = CROSS_PLANE_READ(position_engine.gross_exposure_);
```

### 4.4 WAL-Before-State Ordering (AD-012)

Every L3 state mutation follows the mandatory two-step sequence (Master §AD-012):

```
1. WAL write (O_DSYNC, fsync-confirmed via io_uring CQE)
2. State mutation (position_, gross_exposure_ update)
```

**No exception exists.** This ordering is not configurable. GC-WAL-001 mechanically blocks any merge where a state mutation precedes its WAL write.

```cpp
// Canonical S3 fill application pattern — must match this structure exactly
void PositionEngine::apply_fill(const FillEvent& fill) {
    // Step 1: Write WAL record FIRST — before any state change
    WalRecord record;
    record.record_type = FILL_APPLIED;
    record.sequence    = next_sequence_++;
    record.instrument_id = fill.instrument_id;
    record.fill_qty    = fill.quantity;   // fixed-point int64_t
    record.fill_price  = fill.price;      // fixed-point int64_t (AD-004)
    record.tsc_before_write = clock_monotonic_raw();
    wal_writer_.write_sync(record);       // Blocks until io_uring CQE confirmed
    record.tsc_after_write = clock_monotonic_raw();

    // Step 2: State mutation — only AFTER WAL is fsync-confirmed
    position_        += fill.signed_qty;
    gross_exposure_.fetch_add(std::abs(fill.signed_qty),
                              std::memory_order_release);  // GC-E-044: release, not relaxed
}
```

### 4.5 Position Reconstruction on Recovery

On any unclean startup (process crash, SIGKILL), the pre-flight protocol runs PF-02 (§6) which:

1. Opens the WAL file and scans all `FILL_APPLIED` records.
2. Reconstructs `reconstructed_position = Σ signed_qty for all FILL_APPLIED`.
3. Compares against the last `POSITION_SYNC` WAL record checkpoint.
4. If `reconstructed_position != checkpoint_position`: **block `preflight_armed`, emit P0 incident.**

This is the empirical proof procedure for CLAIM-INV-001. It must run correctly after every kill-9 injection in the 10× test harness (PROC-KILL9-001).

### 4.6 MassActionReport Protocol (CLAIM-L3-MAR-001)

When a MassActionReport is received from the exchange (cancel-on-disconnect triggered, L4 kill switch activated, or manual operator halt):

1. WAL record type `MASS_CANCEL` written and fsync-confirmed **before** any position state is mutated.
2. `order_tag_` table cleared (all in-flight orders marked cancelled).
3. `gross_exposure_` updated to reflect net position-only exposure.
4. `POSITION_SYNC` WAL record written as session boundary marker.

The ordering — WAL MASS_CANCEL first, state second — is identical to the fill path. GC-WAL-001 is active for this path.

---

## 5. Operational Loop (E2)

*Authoritative source: Master §E2*

### 5.1 Loop Cycle Budget

| Phase | Timing Budget |
|---|---|
| Sense (SPSC pop + GovernanceFlags check) | ≤ 500ns P99 |
| Evaluate (FeatureVector read + signal eval + risk check) | ≤ 10µs P99 |
| Execute (WAL fsync + SPSC push) | ≤ 200µs P99 |
| Audit (async audit append) | ≤ 50µs P99 |
| **Total feed-to-decision** | **≤ 500µs P99** |

### 5.2 Operational Loop Phase Specifications

The four-phase ordering `[Sense → Evaluate → Execute → Audit]` is **constitutionally fixed** (Master §E2.3). No optimization may reorder these phases.

| Phase | Inputs | Processing | Exit Conditions |
|---|---|---|---|
| **Sense** | SPSC ring pop from L0→L1; GovernanceFlags snapshot | Pop `RawMarketEvent`. Check `halt_required` first. Check `preflight_armed`. | `halt_required=1` → TERMINAL_HALT. `preflight_armed=0` → loop suspended. |
| **Evaluate** | FeatureVector (seqlock); `egbos_regime_signal`; `egbos_risk_limits` latches | Read FeatureVector. Evaluate signal. Apply risk limit check against `pos_limit_latch`. No external I/O. | Risk breach → emit zero-size order. Below threshold → no order. |
| **Execute** | Evaluated order decision | Write WAL `ORDER_NEW` via O_DSYNC. Push to `egbos_exec_commands` SPSC. L5 routes to exchange. | WAL write failure → TERMINAL_HALT. Short write → fatal. |
| **Audit** | Completed execution decision + WAL sequence anchor | Append Audit Record to Governance Audit Log (D3.2). Emit latency telemetry to Prometheus. | Audit write failure → P1 incident (execution continues). |

**Constitutional ordering invariants (Master §E2.3):**
1. Governance flag check precedes signal evaluation — an unchecked halt is a capital safety defect.
2. WAL write precedes order transmission — an order without a prior WAL record violates CLAIM-L3-001.
3. Audit record follows committed execution — an audit record that precedes commitment is a false record under P-007.

### 5.3 GovernanceFlags SHM Structure

```cpp
// engine/core/governance_interface.h — authoritative layout
// Read-only access to governance flags SHM region
// Called once per operational loop iteration (E2)
// Returns: GovernanceFlags snapshot — not cached between iterations
struct GovernanceFlags {
  std::atomic<uint8_t> halt_required;    // Set by L4 Safety Monitors
  std::atomic<uint8_t> feed_stale;       // Set by C1 Gap Detection
  std::atomic<uint8_t> preflight_armed;  // Set by E3 Pre-flight Protocol
  std::atomic<int64_t> pos_limit_latch;  // Monotone — only decreases (AD-010)
  uint8_t              _pad[40];         // Pad to 64 bytes — one cache line
};

// INVARIANT: halt_required is checked FIRST in every loop iteration.
// If halt_required == 1, the loop exits before evaluating any signal.
// This ordering is not configurable. See E2 Operational Loop.
```

---

## 6. Pre-flight Protocol (E3)

*Authoritative source: Master §E3*

### 6.1 Pre-flight Philosophy

Pre-flight is a governance gate, not a startup convenience. The `preflight_armed` flag in `/dev/shm/egbos_governance_flags` is the **only** mechanism by which the Operational Loop (E2) is authorized to enter its Execute phase. Until `preflight_armed=1`, the loop runs in Sense-only mode.

A system that passed all pre-flight checks yesterday has **not** passed pre-flight today. Hardware state, WAL integrity, schema versions, and connection topology are all session-scoped properties that must be independently verified on every startup.

### 6.2 Pre-flight Check Registry

All 12 mandatory pre-flight checks must be implemented in S3. A single FAIL in Category 1 or Category 2 blocks `preflight_armed` indefinitely — no timeout, no retry limit, no override.

#### Category 1 — State Validation

| ID | Name | Pass Criterion | Failure Action |
|---|---|---|---|
| PF-01 | WAL Integrity Scan | CRC32C pass on all records from last checkpoint to EOF; sequence monotonic | Block preflight_armed |
| PF-02 | Position Reconstruction | Reconstructed position == Σ FILL_APPLIED (INV-001) | Block + P0 incident |
| PF-03 | Safety Monitor Latch Audit | All monotone latches match last WAL session record | Block preflight_armed |
| PF-04 | Governance Flags Initialization | `/dev/shm/egbos_governance_flags` mapped, mlock confirmed, halt_required=0 only after PF-01+PF-02 pass | Block preflight_armed |
| PF-05 | Claim Expiry Sweep | `govcheck deploy-gate --target production`: zero GC-E-020 or GC-E-022 | Block preflight_armed |

#### Category 2 — Schema Verification

| ID | Name | Pass Criterion | Failure Action |
|---|---|---|---|
| PF-06 | FlatBuffers Schema Freshness | SHA-256 of committed generated FlatBuffers code matches binary | Block (prevents silent wire format mismatch) |
| PF-07 | Registry Entry Signature Verification | All Registry entries verified against `signature` field | Block (prevents config drift) |
| PF-08 | timing_constants.json Hardware Match | Runtime `hardware_id` matches committed `timing_constants.json` | Block (V4 claims require their measurement hardware) |
| PF-09 | ONNX Model Hash + Runtime Version | SHA-256 of each ONNX file matches model registry; runtime version exact match | Block (active from Phase V-F) |

#### Category 3 — Connection Integrity (one retry permitted)

| ID | Name | Pass Criterion | Failure Action |
|---|---|---|---|
| PF-10 | Exchange Connectivity Heartbeat | FIX session RTT < 500µs P99 | Block after 1 retry |
| PF-11 | Market Data Feed Sequence Continuity | Sequence continuous from last session's terminal sequence | Block; recovery snapshot required |
| PF-12 | SHM Region mlock Confirmation | All 8 SHM regions mapped and mlock(2) verified; NUMA socket-0 locality confirmed | Block |

### 6.3 Arming Sequence

```bash
# scripts/preflight.sh — mandatory arming sequence
# MUST execute in order; no parallel execution of Category 1

Phase 1 — State Validation (PF-01 → PF-05): serial
  PF-01: wal_integrity_scan --from-last-checkpoint
  PF-02: position_reconstruct --verify-inv-001
  PF-03: latch_audit --against-wal-session-record
  PF-04: governance_flags_init --require-pf01-pf02-pass
  PF-05: govcheck deploy-gate --target production

Phase 2 — Schema Verification (PF-06 → PF-09): parallel permitted
  PF-06, PF-07, PF-08, PF-09

Phase 3 — Connection Integrity (PF-10 → PF-12): parallel permitted
  PF-10, PF-11, PF-12

Final: set preflight_armed=1 in /dev/shm/egbos_governance_flags
       ONLY after all 12 checks pass
```

---

## 7. Risk Governance Framework (L4)

*Authoritative source: Master §L4A, §L4B, §K4.1*

### 7.1 L4 Architectural Thesis (K4.1)

The out-of-process C++ risk enforcer runs as a standalone process on an isolated CPU core, completely decoupled from the execution engine thread. It monitors position limits, gross notional limits, and message rates by reading shared memory state via `CROSS_PLANE_READ`. If the execution engine blocks or crashes, the risk enforcer detects the missed heartbeat and takes control.

### 7.2 Hard and Soft Limit Allocation (L4A)

The risk profile is split into two operational levels:

| Level | Implementation | Enforcement |
|---|---|---|
| **Hard Risk Limits** | C++ in-process enforcer, runs in order generation loop on Node 0 | Maximum position size, maximum order size, order rate throttles, daily loss limits. Hard breach → immediate emergency cancellation and halt. |
| **Soft Risk Limits** | Python VaR/CVaR risk engine, Node 1, published via Redis Streams (AD-008) | Leverage multiplier, correlation limits, funding rate tracking. Updated every 10 seconds. Stale > 5s → minimal risk profile + P1 alarm (REG-RISK-STALE-001). |

**REG-RISK-STALE-001** — Registry entry governing soft limit stale threshold:
- Production value: ≤ 5000ms
- Governed by: CLAIM-RISK-STALE-001 (V1)
- GC-E-050: Any production value exceeding 5000ms is immediately blocking at G2 (staging deploy gate)

### 7.3 Monotone Latch Invariant (AD-010)

L4 risk enforcer uses **monotone latches only**. Once a safety limit is reduced (position limit, gross exposure ceiling), it cannot be increased by the runtime. Only a HITL operator action with governance audit trail may increase a latch value.

```cpp
// engine/core/risk_enforcer.h — canonical latch update pattern
// pos_limit_latch can only decrease autonomously
void RiskEnforcer::apply_breach(int64_t new_ceiling) {
    int64_t current = governance_flags_.pos_limit_latch.load(
                          std::memory_order_acquire);
    // Monotone: only write if new_ceiling is strictly lower
    while (new_ceiling < current) {
        if (governance_flags_.pos_limit_latch.compare_exchange_weak(
                current, new_ceiling,
                std::memory_order_release,
                std::memory_order_acquire)) {
            break; // Latch lowered
        }
        // current was updated by CAS; retry
    }
}
```

**No automatic re-arming.** An operator reset + CRO signature (L1B) is required to increase any latch.

### 7.4 Quoting State Machine (L4B)

The execution engine operates on a strict finite state machine. Valid transitions (Master §L4B.1):

| Source State | Target State | Trigger Condition | Action Executed |
|---|---|---|---|
| INACTIVE | QUOTING | All pre-flight checks pass (L3C / E3) | Allow passive quote generation |
| QUOTING | HEDGING | Inventory limit exceeded OR VPIN > threshold_warn | Cancel passive quotes; begin passive inventory reduction |
| HEDGING | TAKER_REDUCE | Inventory > 150% limit | Cancel all resting orders; submit aggressive market maker orders |
| QUOTING/HEDGING | EMERGENCY | Daily loss limit breach OR watchdog alert OR manual kill | Bypass execution engine; cancel all resting orders via REST; halt |
| EMERGENCY | INACTIVE | Manual operator reset + CRO signature (L1B) | Clear position states; await initialization checks |

The `EMERGENCY` state bypasses the normal operational loop and routes cancellations directly via REST, independent of the L5 execution router.

### 7.5 Incident Classification (L4D)

| Severity | Description | Triggering Event | Automated Mitigation |
|---|---|---|---|
| **P0 (Critical)** | System-wide outage or extreme risk violation. Capital at immediate risk. | Daily loss limit breach, watchdog thread timeout, hardware kill switch activation. | Trigger emergency cancel bypass; physically cut network transceiver fiber; shutdown engine. |
| **P1 (Major)** | Degraded execution or infrastructure failure with minimal capital risk. | Soft limit stream stale >90s, high VPIN warning, exchange feed disconnect. | Suspend new quoting orders; transition to HEDGING or TAKER_REDUCE. |
| **P2 (Info)** | Operational anomaly with no impact on risk or execution latency. | Telemetry publish fail, non-critical database backup delay. | Retry connection; log anomaly in metadata registry. |

---

## 8. Exchange Connectivity Governance (L4C)

*Authoritative source: Master §L4C*

### 8.1 Kernel Bypass Requirements

- All WebSocket, FIX, and binary connections must run via kernel bypass (DPDK or Solarflare OpenOnload).
- P99 loopback latency must remain below **5µs**.
- Order rate throttling: hardware token bucket rate-limiter, operating independently of the execution engine.

### 8.2 Hardware Kill Switch — AMD Alveo U50 (L4C.1)

An AMD Alveo U50 SmartNIC is deployed inline between the execution server and the exchange transceiver fiber.

**Heartbeat protocol:**
1. Pinned watchdog threads in the execution engine emit a high-frequency UDP heartbeat to the SmartNIC.
2. If the heartbeat misses for **> 50µs**, or a manual kill signal is received, the FPGA's logic immediately shuts off power to the SFP+ transceiver cage lasers.
3. Optical disconnect occurs in **< 10µs**.
4. The connection loss simultaneously triggers the exchange-side Cancel-on-Disconnect (COD) protocol.

**S3 scope for kill switch:**
- Software watchdog heartbeat emission: **in scope** (CLAIM-L4-001 kill switch < 1ms under TSAN).
- FPGA RTL authoring for the Alveo U50: **out of scope** for S3 software gate; hardware engineering workstream.
- The TSAN test for CLAIM-L4-001 verifies the C++ software kill switch path, not the physical FPGA path.

---

## 9. Trading Audit Framework (L4E)

*Authoritative source: Master §L4E*

### 9.1 Merkle Chain Audit Log (L4E.1)

All execution decision events, order submissions, fills, and risk limit evaluations must be written to an immutable audit trail. Log entries are structured as a SHA-256 Merkle chain. Each block is signed with the server's private key.

```cpp
// Chaining audit log records with SHA-256
void chain_audit_block(AuditBlock& current_block, const uint8_t* prev_block_hash) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, prev_block_hash, 32);
    SHA256_Update(&ctx, current_block.data, current_block.data_len);
    SHA256_Final(current_block.hash, &ctx);
}
```

### 9.2 WORM Retention (L4E.2)

- Merkle root hash of daily audit logs published to an external immutable ledger or public timestamping service.
- Logs stored on WORM storage appliance with hardware-enforced write locks.
- Minimum retention: **five years**.
- S3 scope: Audit log chaining and SHA-256 implementation. WORM storage provisioning is an S6/S7 infrastructure concern.

---

## 10. Shared Memory Interface Contracts

*Authoritative source: Master §E1.3, §C3*

| Module | Reads From | Writes To | Governance Plane |
|---|---|---|---|
| **Position Engine (L3)** | `/dev/shm/egbos_exec_commands` (SPSC fills from L5) | `WAL` (`FILL_APPLIED`, `MASS_CANCEL`, `POSITION_SYNC`); `/dev/shm/egbos_position_state` (`gross_exposure_` only, atomic) | Reads `halt_required`, `feed_stale` |
| **Risk Enforcer (L4)** | `/dev/shm/egbos_position_state` (`gross_exposure_` via `CROSS_PLANE_READ` only); `REG-RISK-001` | `/dev/shm/egbos_risk_limits` (monotone latches); Governance Audit Log | Reads and writes `halt_required` |
| **Latency Monitor** | All SPSC ring timestamps; WAL `tsc_before_write`/`tsc_after_write`; `timing_constants.json` | Prometheus metrics endpoint; Governance Audit Log on breach | None — read-only observer |

**All SHM region sizes are `constexpr` constants in `engine/core/shm_layout.h` (AD-006). No runtime sizing.**

---

## 11. Govcheck Rules Active in S3

*Authoritative source: Master §B1.3.3, §I2-S3*

| Rule ID | Description | Gate | Severity | Spec Reference |
|---|---|---|---|---|
| GC-E-010 | malloc/new/free/delete in engine/ hot path | P2→P3 | Error | ADR-006; active from S0 |
| GC-E-021 | R4 claim without waiver on deploy-gate | G1/G2 | Error | Required before staging deploy |
| GC-E-043 | Direct access to `order_tag_`/`x_l_` outside L3 | P1 | Error | Memory safety; blocks immediately |
| GC-E-044 | `memory_order_relaxed` on cross-plane atomic | P4 | Error | TSAN requirement; CLAIM-L3-004 |
| GC-E-045 | Direct `x_l_`/`order_tag_` read from L4 or L5 | P1 | Error | CLAIM-L3-006; blocks immediately |
| GC-E-050 | `REG-RISK-STALE-001` production value > 5000ms | G2 | Error | Institutional risk control standard |
| GC-WAL-001 | WAL write not fsync-confirmed before state mutation | C0 | Error | Active from S1; S3 extends to FILL_APPLIED, MASS_CANCEL paths |
| GC-E-020 | `review_expiry` exceeded | C0 | Error | 90-day default expiry; swept in PF-05 |
| GC-E-053 | V3+ claim amended without accepted RFC | C0 | Error | RFC discipline; active from S1 |

---

## 12. Claim Registry for S3

*Authoritative source: Master §B1.2.5, §I4*

| Claim ID | Domain | Target Class | Proving Gate | Deployment Blocking | Proof Standard |
|---|---|---|---|---|---|
| CLAIM-L3-001 | exec.wal_ordering | V4 | P3, P4 | No capital gate in S3 | WAL-before-state 10× kill-9; `position = Σ FILL_APPLIED` |
| CLAIM-INV-001 | exec.position | V4 | P3 | TERMINAL_HALT trigger | 10M fills + 10× kill-9 survival |
| CLAIM-L3-003 | exec.state_ownership | V3 | P1 (GC-E-043 blocking) | — | `order_tag_` private, no public getter |
| CLAIM-L3-004 | exec.memory_ordering | V4 | P4 (GC-E-044 blocking) | — | TSAN-clean + CROSS_PLANE_READ macro documented |
| CLAIM-L3-006 | exec.atomic_exposure | V4 | P4 (GC-E-045 blocking) | — | `gross_exposure_` sole cross-plane read |
| CLAIM-L3-MAR-001 | exec.mass_cancel | V4 | P3 | — | MassActionReport WAL-before-state verified |
| CLAIM-L4-001 | risk.kill_switch | V4 | P4 | IV-A precondition | Kill switch < 1ms response time under TSAN |
| CLAIM-RISK-STALE-001 | risk.soft_limit_stale | V1 | S3 | G2 blocking | Soft risk limit stream stale ≤ 5s; REG-RISK-STALE-001 |

**Carry-forward claims (proved in S1/S2, remain active):**

| Claim ID | Current Class | Carry-Forward Requirement |
|---|---|---|
| CLAIM-WAL-001 | V4 | WAL foundation; all S3 WAL paths depend on it |
| CLAIM-SPSC-001 | V4 | SPSC rings used in L3/L4 SHM paths |
| CLAIM-L3-POOL-001 | V2 | Pool allocator active in L3 hot path |
| CLAIM-LOB-001 | V4 | LOB output feeds L3 via SPSC |

---

## 13. CI Gate Requirements

*Authoritative source: Master §B1.4, §I2-S3*

**S3 exit requires all of the following CI gates to be green:**

| Gate | Name | Description |
|---|---|---|
| **C0** | govcheck validate | `govcheck validate --scope C0 --strict` — zero errors, zero warnings |
| **P1** | Format / Lint / Import | clang-format, clang-tidy, import-linter — must pass on every merge |
| **P2** | Build Matrix | GCC13 + Clang17 + ASAN + UBSAN across all targets |
| **P3** | Unit + Integration | Position engine unit tests; invariant tests; kill-9 harness; MAR WAL tests |
| **P4** | TSAN | ThreadSanitizer — zero races in position engine and risk enforcer under concurrent load |
| **P5** | Latency Regression | WAL P99 within 10% of `timing_constants.json` baseline for FILL_APPLIED path |
| **G1** | Staging Deploy Gate | `govcheck deploy-gate --target staging` — all C0+P gates green; CLAIM-L4-001 at V4 |

**S3 acceptance gate command:**
```bash
govcheck validate --scope C0,P3,P4,P5,G1 --strict
```

---

## 14. ADR Dependencies

*Authoritative source: Master §H (ADR catalog)*

| ADR ID | Title | Status | S3 Dependency |
|---|---|---|---|
| **AD-004** | Fixed-Point Arithmetic Mandate in Execution Plane | active | All L3 position arithmetic uses `int64_t` fixed-point at `PRICE_SCALE = 100,000`. Float is forbidden on hot path. |
| **AD-005** | io_uring Single-CQE-Consumer Invariant | active | WAL writer for FILL_APPLIED, MASS_CANCEL uses the same single-consumer constraint as ORDER_NEW (S1). |
| **AD-006** | SHM Region Sizes as Compile-Time Constants | active | `egbos_position_state` and `egbos_risk_limits` region sizes are `constexpr`; no runtime sizing. |
| **AD-007** | Stale Feed Response — Position Freeze Not Force-Close | active | On `FEED_STALE`, L4 sets new-position limit to zero. Existing positions are NOT force-closed. |
| **AD-008** | Redis Streams as Exclusive IPC Bridge for L6/L7 Python | active | Python VaR/CVaR engine publishes soft limits to C++ via Redis Streams exclusively. |
| **AD-010** | Monotone Latch as Exclusive Safety Enforcement Mechanism in L4 | active | All L4 risk limit reductions are monotone; no automatic re-arming. |
| **AD-012** | WAL-Before-State as Mandatory Write Ordering in L3 | active | Constitutional ordering for all L3 state mutations: FILL_APPLIED, MASS_CANCEL, POSITION_SYNC. |

**Reminder:** No ADR may be changed without CTO + Governance Board approval and a formal RFC (for V3+ claims, per GC-E-053).

---

## 15. Forbidden Actions

From Master §I2-S3. The following actions are mechanically blocked by govcheck or constitutionally prohibited:

1. Any direct read of `x_l_` or `order_tag_` from L4 or L5 — **GC-E-045 blocks at P1**.
2. Any `memory_order_relaxed` on a cross-plane atomic — **GC-E-044 blocks at P4**.
3. Any L3 position state mutation without a prior fsync-confirmed WAL write — **GC-WAL-001 blocks at C0**.
4. Any SHM region sized at runtime — **GC-E-010 / AD-006 block at P2**.
5. Any L4 monotone latch increase without HITL operator action + CRO signature — **AD-010 architectural invariant**.
6. Any `EMERGENCY → INACTIVE` state transition without manual operator reset + CRO signature — **L4B.1 state machine rule**.
7. Any merge skipping clang-format — **P1 hard block**.
8. Any V3+ claim amended without an accepted RFC YAML in `artifacts_changed` — **GC-E-053 blocks at C0**.
9. Any S4 work opened before S3 acceptance gate is signed.
10. Any live exchange connection — this is S5 scope.
11. Any `timing_constants.json` empirical measurement — this is S6 scope.

---

## 16. Invariants

| Invariant ID | Statement | Governing Claim | Enforcement |
|---|---|---|---|
| **INV-001** | `reconstructed_position == Σ FILL_APPLIED` at any WAL checkpoint | CLAIM-INV-001 (V4) | PF-02 pre-flight; TERMINAL_HALT on delta; 10× kill-9 proof |
| **INV-002** | `order_tag_` and `x_l_` are never readable outside L3 | CLAIM-L3-003 (V3), CLAIM-L3-006 (V4) | GC-E-043, GC-E-045 at P1 |
| **INV-003** | `gross_exposure_` is the sole cross-plane atomic; CROSS_PLANE_READ is its only read path | CLAIM-L3-004 (V4), CLAIM-L3-006 (V4) | GC-E-044, GC-E-045 at P1/P4 |
| **INV-004** | Every FILL_APPLIED and MASS_CANCEL WAL write precedes the corresponding state mutation | CLAIM-L3-001 (V4), CLAIM-L3-MAR-001 (V4) | GC-WAL-001 at C0 |
| **INV-005** | Soft limit stream stale ≤ 5 seconds; violation triggers minimal risk profile + P1 | CLAIM-RISK-STALE-001 (V1) | GC-E-050 at G2; REG-RISK-STALE-001 |
| **INV-006** | Kill switch response < 1ms P99 under TSAN load | CLAIM-L4-001 (V4) | P4 TSAN gate; IV-A precondition |
| **INV-007** | All monotone latches only decrease autonomously | AD-010 | Architectural; no govcheck rule required |
| **INV-008** | `halt_required` is checked first in every operational loop iteration | Master §E2.3 constitutional | No govcheck rule; structural contract of E2 implementation |

---

## 17. Interfaces

### 17.1 L3 → WAL Interface

The L3 position engine writes to the WAL using the `WalWriter` from S1. No new WAL interface methods are introduced; only new `record_type` enum values are added to `wal_writer.h`.

### 17.2 L4 → L3 Interface (Cross-Plane Read Only)

L4 reads L3 state via `CROSS_PLANE_READ(gross_exposure_)` **exclusively**. No function calls, no locks, no shared data structures beyond the atomic. This interface is the entire L3↔L4 contract.

### 17.3 L4 → Governance Interface

L4 writes to `/dev/shm/egbos_governance_flags` (`halt_required`, `pos_limit_latch`). L3 reads these flags at the start of every operational loop iteration. The `GovernanceFlags` struct is the full interface — no other inter-process channel is permitted.

### 17.4 Python VaR Engine → L4 Interface

The Python risk engine publishes soft limit updates to L4 exclusively via **Redis Streams** (AD-008). L4 reads from the Redis Stream, validates the soft limit payload, and writes the new ceiling to `/dev/shm/egbos_risk_limits`. The staleness timer (`REG-RISK-STALE-001`) is the L4 watchdog on this stream.

---

## 18. Test Requirements

### 18.1 Mandatory Tests

| Test | Description | Claim Proved | Gate |
|---|---|---|---|
| `position_kill9_test` | 10× SIGKILL injection during FILL_APPLIED write; verify PF-02 recovery each time | CLAIM-L3-001 (V4), CLAIM-INV-001 (V4) | P3 |
| `10m_fill_test` | 10,000,000 synthetic fill events; verify INV-001 at end | CLAIM-INV-001 (V4) | P3 |
| `wal_mass_cancel_test` | MASS_CANCEL WAL-before-state ordering under kill-9 | CLAIM-L3-MAR-001 (V4) | P3 |
| `kill_switch_tsan_test` | L4 kill switch response time < 1ms under TSAN thread contention | CLAIM-L4-001 (V4) | P4 |
| `cross_plane_read_tsan_test` | `gross_exposure_` cross-plane reads via CROSS_PLANE_READ; zero TSAN races | CLAIM-L3-004 (V4) | P4 |
| `latency_regression_fill` | FILL_APPLIED WAL P99 within 10% of `timing_constants.json` baseline | CLAIM-L3-001 timing | P5 |

### 18.2 Fixture Tests (Govcheck)

All S3 govcheck fixtures must fire correctly (error detected) in CI:

| Fixture | Rule Tested |
|---|---|
| `violation_GC_E_043.cpp` | Direct `order_tag_` access from outside L3 → GC-E-043 fires |
| `violation_GC_E_044.cpp` | `memory_order_relaxed` on cross-plane atomic → GC-E-044 fires |
| `violation_GC_E_045.cpp` | Direct `x_l_` read from L4 → GC-E-045 fires |

### 18.3 PROC-KILL9-001 Compliance

The kill-9 test for position engine correctness follows the same PROC-KILL9-001 standard used in S1 (CLAIM-WAL-001). 10 trials minimum. Each trial:
1. Pre-populate WAL with N FILL_APPLIED records.
2. SIGKILL process mid-write.
3. Restart; run PF-01 + PF-02.
4. Verify: `reconstructed_position == Σ FILL_APPLIED` exactly.
5. Record: trial result, WAL record count, latency of PF-02 reconstruction.

---

## 19. Verification Classes

*Authoritative source: Master §B1.2.2*

| Class | Meaning |
|---|---|
| **V1** | Structural / static assertion (compile-time or govcheck rule enforcement) |
| **V2** | Unit test under ASAN — deterministic, no empirical timing |
| **V3** | Govcheck rule blocking; static analysis gate on CI |
| **V4** | Empirical — requires measured proof artifacts under defined conditions (kill-9, TSAN, latency) |

All S3 primary claims target **V4**. CLAIM-L3-003 targets V3 (structural + GC-E-043 blocking). CLAIM-RISK-STALE-001 targets V1 (registry entry + GC-E-050 governance rule).

---

## 20. Exit Criteria (Acceptance Gate)

*Authoritative source: Master §I2-S3*

### 20.1 Acceptance Gate Command

```bash
govcheck validate --scope C0,P3,P4,P5,G1 --strict
```

Zero errors. Zero warnings. All listed claims at their declared target class.

### 20.2 Pass Criteria (All Must Hold)

- [ ] CLAIM-L3-001 at V4 — WAL-before-state 10× kill-9; `position = Σ FILL_APPLIED` on recovery
- [ ] CLAIM-INV-001 at V4 — 10M fills processed; 10× kill-9 survival; INV-001 holds on every recovery
- [ ] CLAIM-L3-003 at V3 — `order_tag_` private; no public getter; GC-E-043 fixture fires
- [ ] CLAIM-L3-004 at V4 — TSAN-clean; CROSS_PLANE_READ macro in place; GC-E-044 fixture fires
- [ ] CLAIM-L3-006 at V4 — `gross_exposure_` is sole cross-plane read; GC-E-045 fixture fires
- [ ] CLAIM-L3-MAR-001 at V4 — MassActionReport WAL-before-state verified under kill-9
- [ ] CLAIM-L4-001 at V4 — Kill switch < 1ms response P99 under TSAN
- [ ] CLAIM-RISK-STALE-001 at V1 — REG-RISK-STALE-001 registry entry present; value ≤ 5000ms; GC-E-050 active
- [ ] G1 gate passing — `govcheck deploy-gate --target staging` green

### 20.3 Carry-Forward Verification

Before signing S3 exit gate, verify:
- [ ] CLAIM-WAL-001 still at V4 (no regression from FILL_APPLIED path extension)
- [ ] CLAIM-SPSC-001 still at V4 (no regression)
- [ ] CLAIM-L3-POOL-001 still at V2 (ASAN clean in L3 pool paths)
- [ ] CLAIM-LOB-001 still at V4 (no LOB regressions from L3 SHM changes)

---

## 21. Entry Conditions

**S3 may not begin entry criteria verification until all of the following are true:**

- [ ] S2 acceptance gate passed and signed.
- [ ] `govcheck validate --scope C0,P3,P4,P5,P6 --strict` exits 0 (S2 gate command).
- [ ] CLAIM-LOB-001 at V4.
- [ ] CLAIM-WAL-001 at V4 (carry-forward from S1).
- [ ] CLAIM-SPSC-001 at V4 (carry-forward from S1).
- [ ] No open P0 incidents.

---

## 22. Failure Mode Specifications

### 22.1 TERMINAL_HALT

`TERMINAL_HALT` is triggered by the following conditions in L3/L4. It is **not recoverable without HITL authorization** (REC-003):

| Trigger | Source | Recovery Record Required |
|---|---|---|
| INV-001 violation (PF-02 detects `reconstructed_position != Σ FILL_APPLIED`) | Pre-flight | RECOVERY-RECORD with RFP delta=0; govcheck G2 pass; INV-001 re-verified V4 |
| WAL write failure in Execute phase | Runtime | RECOVERY-RECORD; WAL replay; RFP reconciliation |
| `halt_required=1` with `preflight_armed=1` in live session | Runtime | Full P0 protocol per Master §A2.3 |

### 22.2 EMERGENCY State Recovery

The `EMERGENCY → INACTIVE` transition requires:
1. Manual operator reset.
2. CRO digital signature (L1B governance record).
3. `preflight_armed` cleared and full PF-01 through PF-12 cycle re-run.
4. Exchange COD confirmation that all resting orders are cancelled.

### 22.3 Soft Limit Stale Degraded Mode (AD-007)

On `REG-RISK-STALE-001` breach (soft limit stream stale > 5 seconds):
1. L4 sets new-position limit to zero via monotone latch.
2. Existing positions are **NOT** force-closed (AD-007).
3. P1 alarm fired.
4. Engine enters `HEDGING` state.
5. Soft limit stream must recover and be validated before latch is manually raised by operator.

---

## 23. Exit Gates for S3

### 23.1 Gate: Acceptance Gate (S3-exit)

**Trigger:** All pass criteria in §20.2 satisfied.  
**Command:** `govcheck validate --scope C0,P3,P4,P5,G1 --strict`  
**Signed by:** Founder (CTO)  
**Effect:** Opens S4 entry window. Opens S8–S15 parallel Research Platform window.

### 23.2 Gate: G1 Staging Deploy Gate

**Trigger:** S3 acceptance gate signed. All S3 claims at declared class.  
**Command:** `govcheck deploy-gate --target staging`  
**Condition:** No R4 (V4 unproved) claims in `deploy-blocking: true` without waiver (GC-E-021).  
**Signed by:** CTO + CRO  
**Effect:** Enables staging infrastructure provisioning for S4.

---

## Appendix A — Conflict Resolution

This document is derived exclusively from `docs/EGBOS_MASTER.md` (converted from `EGBOS_MASTER_augmented.html` Freeze Authority Edition v1.0, Certification ID: EGBOS-FREEZE-2026-001).

In the event of any conflict between:
- This document and `docs/EGBOS_MASTER.md` → **the Master prevails unconditionally**
- This document and any audit report, chat message, or informal note → **this document prevails**
- This document and any other spec document → **resolve via Master §Authority Resolution Order**

*Document ID: EGBOS-SPEC-S3-001 | Source: docs/EGBOS_MASTER.md | Conflict Rule: Master supersedes all*
