# EGBOS — S1 Institutional Implementation Specification
## WAL + SPSC + SHM Skeleton

---

| Field | Value |
|---|---|
| **Document ID** | EGBOS-SPEC-S1-001 |
| **Classification** | Implementation Authority Document — Tier 0 |
| **Status** | ACTIVE — Authoritative for S1 |
| **Owner** | Chief Architecture Office |
| **Approval Authority** | CTO (Founder) |
| **Source of Truth** | `EGBOS_MASTER_augmented.html` v1.0 — Freeze Authority Edition |
| **Governing Principle** | P-004 (Governance Precedes Implementation) |
| **Supersedes** | Any prior informal S1 description in chat, notes, or generated documents |
| **Conflict Rule** | If any statement in this document conflicts with the HTML, the HTML prevails |

---

## Table of Contents

1. [Objectives and Scope](#1-objectives-and-scope)
2. [Architectural Overview](#2-architectural-overview)
3. [Repository Structure](#3-repository-structure)
4. [WAL Architecture](#4-wal-architecture)
5. [SPSC Ring Architecture](#5-spsc-ring-architecture)
6. [Shared Memory Layout](#6-shared-memory-layout)
7. [Memory Allocation Policy](#7-memory-allocation-policy)
8. [Govcheck Rules Active in S1](#8-govcheck-rules-active-in-s1)
9. [Claim Registry for S1](#9-claim-registry-for-s1)
10. [CI Gate Requirements](#10-ci-gate-requirements)
11. [ADR Dependencies](#11-adr-dependencies)
12. [Forbidden Actions](#12-forbidden-actions)
13. [Invariants](#13-invariants)
14. [Interfaces](#14-interfaces)
15. [Test Requirements](#15-test-requirements)
16. [Verification Classes](#16-verification-classes)
17. [Exit Criteria (Acceptance Gate)](#17-exit-criteria-acceptance-gate)
18. [Entry Conditions](#18-entry-conditions)
19. [Failure Mode Specifications](#19-failure-mode-specifications)
20. [Exit Gates for S1](#20-exit-gates-for-s1)

---

## 1. Objectives and Scope

### 1.1 Slice Identity

**S1 — WAL + SPSC + SHM Skeleton** is the second implementation slice in the Execution Plane registry (S0–S7), mapped to ENGINE_V3 Phase 1 and A2.2 Operating Phase I (Infrastructure Qualification). It carries no capital gate; no real or staging capital may be deployed as a consequence of S1 completion.

S1 is the foundational durability and IPC layer on which all subsequent execution-plane slices (S2–S7) depend. Every claim proved in S1 propagates forward as a dependency. No S2 work may open until the S1 acceptance gate passes.

### 1.2 Objectives

S1 must deliver, at the required verification class, three independent subsystems that together constitute the durability and inter-layer communication skeleton of ENGINE_V3:

1. **Write-Ahead Log (WAL)** — the singular source of truth for all position-affecting state transitions. Its correctness is the most critical empirical claim in the entire system (HTML §C2.1).

2. **SPSC Ring Buffer IPC** — the primary lock-free inter-layer communication mechanism between all adjacent execution-plane layers L0–L5 via POSIX shared memory.

3. **Shared Memory Region Skeleton** — the named POSIX SHM region map that defines all fixed-size inter-layer communication regions. All sizes are compile-time constants; no runtime region creation is ever permitted.

### 1.3 Scope

**In scope for S1:**

- `engine/core/wal_writer.h` and its implementation: the WAL writer with `io_uring` integration and O_DSYNC semantics
- `engine/core/spsc_ring.h`: the templated SPSC ring buffer used by all execution-plane layers
- `engine/core/shm_layout.h`: compile-time SHM region constants (already bootstrapped in S0; S1 makes them production-correct)
- `engine/core/cross_plane.h`: the `CROSS_PLANE_READ` macro and atomic access protocol
- WAL recovery logic invoked on unclean startup
- Pool allocator (CLAIM-L3-POOL-001) proof at V2 under ASAN
- Fixture tests for every GC-E rule activated in S1

**Explicitly out of scope for S1:**

- LOB engine (L1) — this is S2
- Feature engine (L2) — this is S2+
- Position engine (L3) — this is S3
- Risk enforcer (L4) — this is S3
- Any live exchange connection — this is S5
- `timing_constants.json` empirical measurement on production hardware — this is S6
- Redis production deployment — this is S7

---

## 2. Architectural Overview

S1 constructs the bottom two layers of the five-plane architecture (HTML §Architecture Visualizations):

```
LAYER 2: EXECUTION PLANE (partial — S1 scope)
┌─────────────────────────────────────────────────────────────────┐
│  [S1]  WAL Writer │ SPSC Rings  │ SHM Layout  │ Pool Allocator  │
│         ↑                                                        │
│    io_uring + O_DSYNC                                            │
└─────────────────────────────────────────────────────────────────┘
                    ↓ capital flows (future slices) ↓
LAYER 3: DURABILITY PLANE (partial — S1 WAL primary)
┌─────────────────────────────────────────────────────────────────┐
│  WAL Primary (NVMe Gen4)  │  WAL Shadow (NVMe Gen3)             │
│  Stratum-0 Emergency      │  INV-001..005 (partial S1)          │
└─────────────────────────────────────────────────────────────────┘
```

The central architectural thesis governing S1 is from HTML §C2.1:

> _The WAL is not a log of what happened — it is the **definition of what happened**. If a state transition is not in the WAL, it did not happen from the perspective of the governance system._

All WAL-before-state ordering requirements, all SPSC single-producer/single-consumer constraints, and all compile-time SHM sizing requirements must be structurally enforced by govcheck rules — not by convention or discipline. P-001 (Mechanical Enforcement Supremacy) applies fully.

---

## 3. Repository Structure

S1 artifacts live within the directory structure established by S0. The following paths are authoritative (HTML §C3, §C2, §B1.8):

```
engine/
  core/
    wal_writer.h              # WAL writer interface — io_uring, O_DSYNC
    spsc_ring.h               # Templated SPSC ring; compile-time slot size
    shm_layout.h              # constexpr SHM region sizes (AD-006)
    cross_plane.h             # CROSS_PLANE_READ macro (CLAIM-L3-004)
    pool_allocator.h          # Fixed-size pool; no malloc/new on hot path

governance/
  claims/
    CLAIM-WAL-001.yaml        # WAL durability — target V4
    CLAIM-SPSC-001.yaml       # SPSC correctness — target V4
    CLAIM-L3-POOL-001.yaml    # Pool allocator — target V2
  proof_artifacts/
    wal_kill9_test_results/   # 10× kill-9 trial records (V4 evidence)
    spsc_tsan_results/        # TSAN run log (V4 evidence)
    timing_constants.json     # Will be populated in S6; present as schema stub

tests/
  governance/
    fixtures/
      violation_GC_E_010.cpp  # malloc in engine/ — must fire GC-E-010
      violation_GC_WAL_001.cpp # WAL write without fsync-first — must fire GC-WAL-001
  unit/
    wal_writer_test.cpp        # WAL unit test suite
    spsc_ring_test.cpp         # SPSC unit + TSAN test suite
    pool_allocator_test.cpp    # ASAN pool allocator test

scripts/
  run_kill9_wal_test.sh        # Reproducible kill-9 injection harness
```

**Namespace conventions (HTML §B1.8):**
- `wal.*` — WAL durability and ordering claims
- `ipc.*` — IPC mechanism correctness (spsc, shm)
- `allocator.*` — Memory allocation correctness
- `bootstrap.*` — System bootstrap (inherited from S0)

All claim files must use `schema_version: 1`. All `owner` fields must reference a GitHub username present in `governance/team.yaml`.

---

## 4. WAL Architecture

*Authoritative source: HTML §C2 — Write-Ahead Log (WAL) Architecture*

### 4.1 WAL Architectural Thesis

The WAL-before-state ordering is the central correctness property of the execution plane. From HTML §C2.1:

> _CLAIM-L3-001 (V4) proves via 10× kill-9 injection that no in-memory state mutation is observable after recovery that was not preceded by a WAL record. This is the most critical empirical claim in the entire system._

### 4.2 Physical Storage Topology

| Layer | Device | fsync Policy | Role |
|---|---|---|---|
| WAL Primary | NVMe Gen4 (≥3GB/s write) | Per-record O_DSYNC | Authoritative — recovery source |
| WAL Shadow | Second NVMe (same chassis) | Async mirror (100ms lag) | Hardware failure failover only |
| Stratum-0 | SATA SSD (separate controller) | Per-session fsync | Emergency recovery — controller failure |

**S1 note:** WAL Primary and Shadow are required. Stratum-0 is architecturally specified here but its cold recovery drill is an S6 acceptance criterion (HTML §I2 slice-S6). The shadow write path must be implemented and tested in S1 but the hardware-specific SLA measurement belongs to S6.

### 4.3 WAL Record Schema

The WAL record is a fixed-size 64-byte header plus variable payload (max 256 bytes). This schema is canonical and may not be changed without an ADR (HTML §C2.2):

```cpp
// WAL record layout — fixed-size header + variable payload
// Total wire size: 64 bytes header + payload (max 256 bytes)
struct WalRecord {
  // Header (64 bytes, cache-line aligned)
  uint64_t  sequence;          // Monotonically increasing; never reset
  uint64_t  tsc_before_write;  // CLOCK_MONOTONIC_RAW before fsync
  uint64_t  tsc_after_write;   // CLOCK_MONOTONIC_RAW after fsync (latency audit)
  uint32_t  record_type;       // Enum: ORDER_NEW / FILL_APPLIED / POSITION_SYNC /
                               //       MASS_CANCEL / REGIME_CHANGE / SESSION_START /
                               //       HITL_HALT_CONFIRM / HITL_RESTART_AUTH /
                               //       HITL_RISK_LIMIT_CHANGE /
                               //       HITL_SIGNAL_WEIGHT_CHANGE / HITL_REGISTRY_EMERGENCY_AMEND /
                               //       CAPITAL_GATE_CLOCK_START
  uint32_t  instrument_id;
  uint64_t  order_id;          // 0 for non-order records
  int64_t   quantity_delta;    // Signed — positive=buy, negative=sell
  int64_t   price;             // Fixed-point PRICE_SCALE
  uint32_t  payload_length;    // 0–256 bytes
  uint32_t  header_crc32c;     // CRC32C of header fields above
  uint8_t   _pad[8];           // Pad to 64 bytes

  // Payload (variable, immediately following header)
  uint8_t   payload[0];        // Flexible array; payload_crc32c is payload[last-4..last]
};
```

**Invariants on the schema:**
- `PRICE_SCALE = 100,000` is a compile-time constant. Changing it requires an ADR.
- Floating-point is structurally forbidden in all WAL price fields.
- `tsc_before_write` and `tsc_after_write` use `CLOCK_MONOTONIC_RAW` — `CLOCK_REALTIME` is forbidden (GC-E-011).

### 4.4 Record Type Enumeration

The following `record_type` values are defined for S1 scope. Additional types added in later slices require a new YAML enum registration — they may not be added ad hoc:

| Type | Phase Introduced | Description |
|---|---|---|
| `ORDER_NEW` | S3 | New order placed |
| `FILL_APPLIED` | S3 | Fill applied to position |
| `POSITION_SYNC` | S3 | Position state checkpoint |
| `MASS_CANCEL` | S3 | MassActionReport WAL record |
| `REGIME_CHANGE` | S3+ | Regime classifier transition |
| `SESSION_START` | S1 | WAL session begin (clean startup) |
| `HITL_HALT_CONFIRM` | S4+ | Human-in-the-loop halt confirmed |
| `HITL_RESTART_AUTH` | S4+ | Human-in-the-loop restart authorized |
| `HITL_RISK_LIMIT_CHANGE` | S4+ | Risk limit changed via HITL |
| `HITL_SIGNAL_WEIGHT_CHANGE` | S4+ | Signal weight changed via HITL |
| `HITL_REGISTRY_EMERGENCY_AMEND` | S4+ | Emergency registry amendment |
| `CAPITAL_GATE_CLOCK_START` | S7 | Founder confirms capital gate clock start |

**S1 must implement:** `SESSION_START` as the clean-startup marker enabling recovery to distinguish clean vs unclean shutdown.

### 4.5 io_uring Integration and Single-CQE-Consumer Constraint

The WAL writer uses `io_uring` with `IORING_OP_WRITE_FIXED` for kernel-bypass buffer registration. The following constraint is **architectural and non-negotiable** (HTML §C2.2, AD-005):

> **A single thread is the sole CQE consumer of the io_uring ring.** This assumption is an explicit dependency of CLAIM-WAL-001. Any multi-threaded CQE consumption requires a new claim at minimum V1 class before the code change may merge.

Implementation requirement: the WAL writer's `submit_and_wait()` path must assert thread identity on every CQE collection. In DEBUG builds this is a runtime check; in release builds it is a compile-time macro. Violation detected at runtime triggers TERMINAL_HALT (P-006).

**O_DSYNC semantics:** The WAL writer opens the WAL file with `O_DSYNC`. After `write()` returns, data is on stable storage. The return value must equal buffer length; short writes are fatal errors — not retryable conditions.

### 4.6 WAL-Before-State Three-Phase Commit Protocol

Every component that mutates position-affecting state must use the following protocol (HTML §C2.3). This is not an implementation detail; it is a constitutional invariant enforced by `GC-WAL-001`:

```cpp
// Three-phase commit protocol — mandatory for all L3 state mutations
// Phase 1: Prepare WAL record (in-memory, not yet written)
WalRecord rec = build_wal_record(event);

// Phase 2: Write and fsync WAL record
// After this line returns, the record is on stable storage.
// If the process dies after this line, recovery will replay this event.
wal_writer_.write_sync(rec);      // Blocks until O_DSYNC returns

// Phase 3: Apply state mutation
// Only now is in-memory state updated.
// If the process dies before this line, recovery replays Phase 2's record
// and re-applies the mutation — idempotent by construction.
position_state_.apply(event);
```

**Idempotency requirement:** Recovery replays the WAL from the last confirmed checkpoint. Any record written but not applied (crash between Phase 2 and Phase 3) will be replayed and re-applied. **All state mutations must be idempotent**: applying the same WAL record twice must produce the same final state as applying it once.

### 4.7 WAL Recovery Protocol

Recovery is triggered on any unclean startup (no clean shutdown record at end of WAL). In S1, recovery operates against synthetic fixtures only (HTML §A2.2 Phase I). The five-step protocol is (HTML §C2.4):

| Step | Name | Action |
|---|---|---|
| 1 | CRC32C Verification | Scan WAL from last checkpoint. Verify `header_crc32c` on every record. First CRC failure = WAL truncation point. All records after are discarded. |
| 2 | Sequence Gap Detection | Verify sequence monotonicity. Any gap in sequence numbers indicates a partial write. Treat as truncation point — identical to CRC failure. |
| 3 | State Reconstruction | Replay all valid records from checkpoint to truncation point. Apply each record to position state. Result: in-memory state identical to pre-crash state minus at most one in-flight transaction (PROC-REPLAY-001). |
| 4 | RFP Reconciliation | P0 incidents only; compare reconstructed state against exchange RFP. **Blocked until S5 (live feed).** |
| 5 | Recovery Sign-Off | Founder signs `RECOVERY-RECORD-{date}.yaml` with `wal_truncation_point`, `records_replayed`, `position_state_hash`, `rfp_match_status`. Committed to `governance/` before engine restart authorized. **S1: synthetic recovery sign-off only.** |

### 4.8 WAL Invariant Definitions

These invariants are the formal properties S1 must prove (HTML §C2.5):

| Invariant | Formal Definition | Verification Method | Violation Response |
|---|---|---|---|
| **INV-001** | `position == Σ FILL_APPLIED records in WAL for instrument_id` | CLAIM-INV-001 V4: 10M fills + 10× kill-9 | TERMINAL_HALT — P0 incident |
| **WAL-SEQ** | WAL sequence numbers are strictly monotonically increasing — never reset, never reused | CLAIM-WAL-001 V4 | TERMINAL_HALT |
| **WAL-ORDER** | For any state S and WAL record W that caused S, `W.tsc_after_write < tsc(S.first_observable)` | CLAIM-L3-001 V4: kill-9 with TSC comparison | TERMINAL_HALT |
| **WAL-IDEMPOTENT** | `apply(state, WAL[i])` applied twice == `apply(state, WAL[i])` applied once | Recovery replay test — 3× byte-identical position state | Invalidate CLAIM-L3-001; R4 remediation |

**Note on scope:** INV-001 and CLAIM-L3-001 are S3 claims that depend on WAL correctness proved in S1. CLAIM-WAL-001 (V4) is the S1 exit claim. WAL-SEQ and WAL-ORDER are properties CLAIM-WAL-001 must prove at V4.

---

## 5. SPSC Ring Architecture

*Authoritative source: HTML §C3.2 — SPSC Ring Buffer Specification*

### 5.1 Architectural Thesis

SPSC (Single-Producer Single-Consumer) ring buffers are the **primary IPC mechanism** between all adjacent execution-plane layers. Lock-free by construction — no mutex, no atomic CAS on the fast path. Correctness depends absolutely on the single-producer/single-consumer constraint being **structurally enforced**, not disciplinarily expected (CLAIM-SPSC-001).

### 5.2 SPSC Ring Parameter Specification

| Parameter | Value | Rationale |
|---|---|---|
| Ring size | Power of 2 (65536 default) | Modulo as bitmask — no division on hot path |
| Slot size | Fixed at compile time | No dynamic allocation; no pointer chasing |
| Producer index | `std::atomic<uint64_t>` — `seq_cst` on write | Ensures consumer observes complete slot before index advance |
| Consumer index | `std::atomic<uint64_t>` — `relaxed` on read | Consumer owns its index; no cross-thread visibility required |
| Padding | Producer and consumer indices on separate cache lines (64B) | False sharing elimination — CLAIM-SPSC-001 latency claim depends on this |
| Full condition | Producer spins (yield after N iterations) | Backpressure; never drops. Spin budget in `timing_constants.json` |

**Timing target (S1):** SPSC push/pop P99 ≤ 50 ns. This must be verified against `timing_constants.json` baseline under P5 latency regression gate. The S1 `timing_constants.json` is a schema stub; empirical measurements against production hardware are deferred to S6.

### 5.3 Full Condition Behaviour

The SPSC ring **never drops events**. On a full ring, the producer spins with a yield after N iterations (N sourced from `timing_constants.json`). A sustained full condition must be observable via Prometheus counter — the metric registration must be structurally present from S1 forward even if the Prometheus exporter is not yet live (H2 is an S7 concern).

### 5.4 Single-Producer/Single-Consumer Enforcement

The SPSC constraint is not a soft convention. GC-E-044 and CLAIM-SPSC-001 together enforce it:

- `GC-WAL-001` (see §8) enforces that WAL writes precede state mutations
- TSAN (P4 gate) detects data races arising from multiple producers or consumers
- Thread identity assertions on producer/consumer roles must be in the implementation

Any addition of a second producer or second consumer to any SPSC ring constitutes a structural violation requiring a new claim at V1 minimum before the code change merges.

---

## 6. Shared Memory Layout

*Authoritative source: HTML §C3 — Shared Memory Runtime Layout*

### 6.1 Architectural Invariants

- All inter-layer communication passes through named POSIX shared memory regions
- **No Unix sockets, pipes, or TCP connections on the hot path** (HTML §C3.1)
- **SHM layout is fixed at startup** — no runtime region creation ever
- All region sizes are `constexpr` constants in `engine/core/shm_layout.h` (AD-006)
- All regions: `mlock(2)` mandatory — no page faults on hot path (HTML §C3.1)
- All regions: NUMA-local to execution CPU socket

### 6.2 SHM Region Map

The following is the authoritative region map for the execution plane (HTML §C3.1):

```
SHM REGION MAP — ENGINE_V3 EXECUTION PLANE
════════════════════════════════════════════════════════════════════════
/dev/shm/egbos_market_events     [L0 → L1]   SPSC ring, 65536 slots × 128B
/dev/shm/egbos_lob_snapshots     [L1 → L2]   Double-buffered LOB, per instrument
/dev/shm/egbos_feature_vector    [L2 → L3]   256B FeatureVector, seqlock protected
/dev/shm/egbos_position_state    [L3 → L4]   Read-only L4 view (gross_exposure_ only)
/dev/shm/egbos_risk_limits       [L4 → L3]   Monotone latches: kill_switch, pos_limit
/dev/shm/egbos_exec_commands     [L3 → L5]   SPSC ring, 4096 slots × 64B
/dev/shm/egbos_regime_signal     [L6/L7 → L3] Redis Streams bridge output, 1KB
/dev/shm/egbos_governance_flags  [All layers]  Read-only: feed_stale, halt_required
════════════════════════════════════════════════════════════════════════
All regions: mlock(2) mandatory — no page faults on hot path
All regions: NUMA-local to execution CPU socket
```

**S1 requirement:** All eight regions must be defined as `constexpr` constants in `shm_layout.h`. The regions for L1, L2, L3, L4, L5, and L6/L7 communication are structurally defined in S1 even though the layers themselves are implemented in S2–S3. This is required for compile-time correctness checking across the slice boundary.

**Size derivation:** Region sizes are derived from instrument universe size and `timing_constants.json`. In S1, sizes are set to the authoritative defaults shown above. Deviation from these defaults requires an ADR amendment to AD-006.

### 6.3 Cross-Plane Atomic Access Protocol (CLAIM-L3-004, CLAIM-L3-006)

From HTML §C3.3 — this protocol governs how Layer 4 (Risk Enforcer) reads Layer 3 (Position Engine) state:

> _The only piece of L3 state that L4 may read directly is `gross_exposure_` — a single `std::atomic<int64_t>`. All other L3 state is structurally inaccessible to L4: not via accessor, not via pointer, not via shared struct. GC-E-045 enforces this at compile time._

The `CROSS_PLANE_READ` macro (defined in `engine/core/cross_plane.h`) is mandatory for any atomic read crossing a layer boundary. Its prototype:

```cpp
// Cross-plane read macro — mandatory for any atomic read crossing layer boundary
// Defined in engine/core/cross_plane.h
// Usage documents the access and triggers static analysis annotation
#define CROSS_PLANE_READ(atomic_var, layer_from, layer_to) \
  ({ \
    static_assert(sizeof(atomic_var) == sizeof(int64_t), \
      "CROSS_PLANE_READ: only int64_t atomics permitted"); \
    (atomic_var).load(std::memory_order_acquire); \
  })

// GC-E-044 blocks memory_order_relaxed on any CROSS_PLANE_READ
```

**S1 must implement** `cross_plane.h` with this macro even though L3/L4 are S3 concerns, because `GC-E-044` and `GC-E-045` activate at Phase 4 and must have fixture tests registered in S1 (see §8).

### 6.4 FeatureVector Seqlock Protocol

The 256-byte `FeatureVector` (L2 → L3 communication) uses a seqlock. S1 must define the seqlock protocol in `shm_layout.h` even though L2 is implemented in S2. The protocol is (HTML §C3.4):

```cpp
// Seqlock protocol — L2 writer / L3 reader
// Writer:
sequence_.fetch_add(1, seq_cst);  // Odd sequence = write in progress
std::memcpy(feature_vector_, src, 256);
std::atomic_thread_fence(seq_cst);
sequence_.fetch_add(1, seq_cst);  // Even sequence = write complete

// Reader (L3):
uint64_t seq1, seq2;
do {
  seq1 = sequence_.load(acquire);
  if (seq1 & 1) continue;         // Odd = in progress, spin
  std::memcpy(dest, feature_vector_, 256);
  std::atomic_thread_fence(acquire);
  seq2 = sequence_.load(acquire);
} while (seq1 != seq2);           // Torn read detected, retry
```

---

## 7. Memory Allocation Policy

*Authoritative source: HTML §B1.5 (GC-E-010), §A1.6*

### 7.1 Hot-Path Allocation Prohibition

The following are **unconditionally forbidden** in `engine/` hot-path code from Phase 1 (S1) forward:

- `malloc()` / `free()`
- `new` / `delete`
- `std::vector::push_back()` (when resizing — size must be reserved at startup)
- Any heap allocation that occurs after the engine enters its operational loop

**Enforcement:** GC-E-010 activates at Phase 1 (S1) at Warning lifecycle, transitioning to **Blocking at Phase 3** (S3 acceptance gate). S1 must have the rule active in Warning mode with a violation fixture registered.

**Rationale (HTML §P-002):** Structural impossibility over runtime prevention. Dynamic allocation in the hot path creates unpredictable latency spikes that invalidate timing_constants.json measurements and make P5 latency regression meaningless.

### 7.2 Pool Allocator Requirement (CLAIM-L3-POOL-001)

S1 must prove `CLAIM-L3-POOL-001` at **V2** (compile-time or build-time enforcement): the pool allocator serves fixed-size object allocation for WAL records and SPSC slot buffers without calling `malloc`. Requirements:

- Pool is pre-allocated at startup with a fixed capacity
- Under ASAN, zero `malloc`/`new` calls are emitted from pool operation paths
- Pool exhaustion is a fatal startup error — not a runtime condition

---

## 8. Govcheck Rules Active in S1

Rules that become active (Warning or Blocking) during S1. All Blocking rules must have a registered violation fixture in `tests/governance/fixtures/` by S1 close (HTML §B1.4.3).

| Rule ID | Description | S1 Lifecycle | Fixture Required |
|---|---|---|---|
| **GC-E-010** | `malloc`/`new`/`free`/`delete` in `engine/` hot-path | Warning (→ Blocking at Phase 3/S3) | Yes — `violation_GC_E_010.cpp` |
| **GC-WAL-001** | WAL write that is not fsync-confirmed before state mutation | Blocking immediately (HTML §I2 slice-S1) | Yes — `violation_GC_WAL_001.cpp` |
| **GC-E-053** | V3+ claim amended without accepted RFC in `artifacts_changed` list | Blocking immediately at C0 | Yes — `violation_GC_E_053.cpp` |
| **GC-E-054** | Research environment SIMD/FMA flags non-compliant (missing `-ffp-contract=off` or `NPY_DISABLE_SVML=1`, or SIMD exceeds production ceiling) | Blocking at P2 | Yes — `violation_GC_E_054.cpp` |
| **GC-E-SECRETS-001** | Secret material in tracked source files (`KEY=`, `SECRET=`, `PASSWORD=`, `TOKEN=` patterns; exchange API key format in `engine/` or `scripts/`) | Blocking immediately at C0 | Yes — `violation_GC_E_SECRETS_001.sh` |
| **GC-E-SECRETS-002** | Vault endpoint unreachable at session startup (CHK-004 failure path) | Blocking at pre-flight PF-07 | Yes — as pre-flight fixture |
| **GC-E-056** | `SIM_STATE` accesses production SHM, WAL, or Audit Log file descriptors outside simulation telemetry boundary (AD-013 enforcement) | Blocking immediately at C0 | Yes — `violation_GC_E_056.js` |

> **Note on GC-E-056 fixture:** HTML §B1.4.3 explicitly specifies the fixture as a JavaScript file: `tests/governance/fixtures/violation_GC_E_056.js` — a JavaScript module that calls `ObservabilityBridge.dispatch()` with `source: "live"` while `SIM_STATE` references a mock `/dev/shm/` path.

**Rules inherited from S0 (must remain active):**

| Rule ID | Description | Gate |
|---|---|---|
| GC-E-017 | `composed_verification_class` mismatch | C0 |
| GC-E-018 | Proof artifact SHA-256 mismatch | C0 |
| GC-E-019 | Claim owner not in `team.yaml` | C0 |

**Rules that pre-register in S1 for future-slice enforcement (activate at Phase 2/S2 or later, fixture tests required now):**

| Rule ID | Description | Active From | Fixture Required in S1 |
|---|---|---|---|
| GC-E-011 | `clock_gettime(CLOCK_REALTIME)` in `engine/` | Phase 2 (S2), Blocking immediately | Yes — `violation_GC_E_011.cpp` |
| GC-E-044 | `memory_order_relaxed` on cross-plane atomic | Phase 4, Blocking immediately | Yes — `violation_GC_E_044.cpp` |
| GC-E-045 | Direct `x_l_`/`order_tag_` read from L4/L5 | Phase 4, Blocking immediately | Yes — `violation_GC_E_045.cpp` |

---

## 9. Claim Registry for S1

The following claims must be proved in S1 at their declared target verification class. The Claim-to-Slice mapping in HTML §I4 is authoritative.

### 9.1 Claims Proved in S1 (Proving Slice = S1)

| Claim ID | Namespace | Target Class | Evidence Required | CI Gate |
|---|---|---|---|---|
| **CLAIM-WAL-001** | `wal.durability` | **V4** | 10× kill-9 survival (PROC-KILL9-001); fsync P99 ≤ 8,100 ns on target hardware; `SESSION_START` / clean shutdown pair; CRC32C verified header on every record | P3, P4, P5 |
| **CLAIM-SPSC-001** | `ipc.spsc` | **V4** | TSAN-clean full test suite; P99 push/pop ≤ 50 ns on target hardware; producer/consumer index cache-line isolation verified | P4 |
| **CLAIM-L3-POOL-001** | `allocator.pool` | **V2** | Code review evidence; ASAN run produces zero `malloc`/`new` from pool paths | P3 |

### 9.2 Claim Schema Requirements

Every claim YAML must include all fields from the canonical schema (HTML §B1.2.1):

```yaml
schema_version:                1
claim_id:                      "CLAIM-WAL-001"
namespace:                     "wal.durability"
title:                         "<= 120 characters"
text:                          |
  <Precise property being claimed>
verification_class:            "V4"
previous_class:                null
class_rationale:               "<Why V4; what evidence supports it>"
enforcement_mechanism:         "<Exact tool, rule ID, or test name>"
enforcement_scope:             "<What is covered — must be precise>"
enforcement_gaps:              []  # Mandatory; exhaustive list of what is NOT covered
assumptions:                   []  # Exhaustive list of every assumption the claim depends on
composition_dependencies:      []  # Claims this depends on for correctness
composed_verification_class:   "V4"   # Computed by govcheck; human value checked not trusted
owner:                         "@founder"
review_authority:              "@founder"
backup_owner:                  null
remediation_category:          null
remediation_notes:             null
deployment_blocking:           true
deployment_blocking_rationale: "<Why this claim must hold before any deployment>"
minimum_class_for_unblocked_deploy: "V4"
rfc_references:                []  # List of {rfc, section, anchor}
proof_artifacts:
  - path: "governance/proof_artifacts/wal_kill9_test_results/run_YYYYMMDD.log"
    type: "test_log"
    sha256: "<SHA-256 of artifact>"
review_history:                []  # Append-only; {date, reviewer, action, class_at_review, notes}
created_at:                    "<ISO-8601>"
created_by:                    "@founder"
last_reviewed:                 "<ISO-8601>"
review_expiry:                 "<12 months from last_reviewed>"
status:                        "active"
superseded_by:                 null
amendments:                    []  # Append-only amendment records
```

---

## 10. CI Gate Requirements

*Authoritative source: HTML §B1.4 — CI Gate Hierarchy*

S1 exit requires the following CI gates to be active and passing:

```
govcheck validate --scope C0,P3,P4,P5 --strict
```

**Gate execution order (HTML §B1.4.2):**

```
C0 → P1 → P2 → P3 → P4 (conditional) → P5 (conditional) → P6 (nightly)
```

**Gate definitions:**

| Gate | Name | Trigger | Key Checks | On Failure |
|---|---|---|---|---|
| **C0** | Governance Pre-Check | All PRs | Schema, DAG, SHA-256, owner, expiry, waiver, composed class | Hard block |
| **P1** | Format + Lint + Import | All PRs | `clang-format`, `clang-tidy`, `lint-imports`, `verify_all_fixtures.sh` | Hard block |
| **P2** | Build Matrix | All PRs | GCC 13 + Clang 17, ASAN + UBSAN, schema freshness | Hard block |
| **P3** | Unit + Integration | All PRs | ASAN + UBSAN tests, WAL determinism, pool allocator ASAN | Hard block |
| **P4** | ThreadSanitizer | `engine/` PRs | Full suite under TSAN; **zero races permitted** | Hard block |
| **P5** | Latency Regression | Hot-path PRs | P99 WAL fsync and SPSC push/pop within 10% of `timing_constants.json` baseline | Hard block |

**Gate activation rule (HTML §B1.4.1):** At activation, immediately blocking — no grace period. Failure in gate N blocks N+1 through N+k. No gate may be skipped or conditionally disabled.

**P5 baseline for S1:** Because production hardware measurement (`timing_constants.json`) is deferred to S6, P5 in S1 operates against a **development-hardware baseline** established on the build CI machine. The S6 re-measurement will supersede this. The baseline must be committed and version-controlled before P5 can operate.

---

## 11. ADR Dependencies

S1 must comply with the following **mandatory** Architectural Decision Records as stated in the HTML §I2 slice-S1 card. No ADR may be superseded during S1 without a formal RFC (HTML §I6) and a full re-evaluation of all S1 claims.

| ADR | Description (per HTML S1 slice card) | S1 Binding Constraint |
|---|---|---|
| **AD-005** | io_uring single-CQE-consumer | WAL writer io_uring ring has exactly one CQE consumer thread. Any code change requiring a second consumer needs a new V1 claim before merging. |
| **AD-006** | SHM compile-time constants | All region sizes in `shm_layout.h` must be `constexpr`. No runtime configuration of sizes. Changes require recompile. |
| **AD-012** | WAL-before-state ordering | The three-phase commit protocol (§4.6) is the sole authorised pattern for all state mutations. Any write-behind pattern is unconditionally rejected. |

*Source: HTML §I2 slice-S1 — "Mandatory ADRs: AD-005 (io_uring single-CQE-consumer), AD-006 (SHM compile-time constants), AD-012 (WAL-before-state ordering)"*

---

## 12. Forbidden Actions

The following actions are structurally prohibited during S1 as stated verbatim in the HTML §I2 slice-S1 Forbidden Actions:

1. **Any WAL write that is not fsync-confirmed before state mutation.** GC-WAL-001 must block. No exception, no waiver.

2. **Any SHM region sized at runtime.** All sizes must be `constexpr` in `shm_layout.h` per AD-006.

3. **Any CQE consumer thread beyond one on the WAL writer io_uring ring.** Violation of AD-005. Requires new V1 claim before code change merges.

*Source: HTML §I2 slice-S1 — verbatim Forbidden Actions list.*

**Additional blocking actions from active govcheck rules (HTML §B1.5.2):**

4. **`CLOCK_REALTIME` in `engine/` code.** GC-E-011 is Blocking from Phase 2; WAL writer code must comply from S1 forward.

5. **`malloc` / `new` / `free` / `delete` in `engine/` hot-path code.** GC-E-010 is Warning in S1; fixture must exist; advisory must not be suppressed.

6. **Secret material in tracked source files** (`KEY=`, `SECRET=`, `PASSWORD=`, `TOKEN=` patterns, exchange API key format). GC-E-SECRETS-001 blocks at C0.

7. **`SIM_STATE` accessing production SHM, WAL, or Audit Log file descriptors outside the simulation telemetry boundary.** GC-E-056 blocks at C0 per AD-013.

8. **Any merge skipping clang-format.** P1 hard block. Inherited from S0.

9. **Any V3+ claim amendment without an accepted RFC listed in `artifacts_changed`.** GC-E-053 blocks at C0.

---

## 13. Invariants

These invariants define what S1 must prove to be true. They are not aspirational; they are the acceptance criteria expressed as formal properties.

### 13.1 WAL Invariants (proved in S1)

| ID | Statement | Verification |
|---|---|---|
| **WAL-SEQ** | WAL sequence numbers are strictly monotonically increasing — never reset, never reused | CLAIM-WAL-001 V4: kill-9 injection tests confirm sequence continuity across 10 recovery cycles. |
| **WAL-ORDER** | For any state S and WAL record W that caused S: `W.tsc_after_write < tsc(S.first_observable)`. WAL write always precedes state observation. | CLAIM-WAL-001 V4 + CLAIM-L3-001 V4 (S3): TSC comparison across kill-9 boundary. |
| **WAL-IDEMPOTENT** | Replaying WAL record W twice against state S produces the same result as replaying it once. | Recovery replay suite: 3× byte-identical position state after replay (PROC-REPLAY-001). |
| **WAL-CRC** | Every WAL record has a valid CRC32C covering the header fields. A record with a failing CRC is the truncation point; no subsequent record is trusted. | Corruption injection test: intentional bit-flip in `header_crc32c`; recovery correctly truncates. |

### 13.2 SPSC Invariants (proved in S1)

| ID | Statement | Verification |
|---|---|---|
| **SPSC-SINGLE** | Exactly one thread writes to any SPSC ring (producer). Exactly one thread reads from any SPSC ring (consumer). No other thread may access the ring's head or tail indices. | TSAN: zero data races on full suite including stress test (P4). Thread identity assertion fires in DEBUG builds. |
| **SPSC-ORDERING** | A slot is fully written before the producer index advances. A consumer reads a slot only after observing the producer index advance. | CLAIM-SPSC-001 V4: `seq_cst` on producer index write ensures ordering. TSAN confirms. |
| **SPSC-NO-DROP** | The SPSC ring never silently drops an event. On full, the producer spins. | Stress test: 650K events/sec sustained for 60 s (10× nominal); zero dropped events under TSAN. |
| **SPSC-CACHE-LINE** | Producer index and consumer index reside on separate 64-byte cache lines. False sharing is structurally impossible. | Static assertion in `spsc_ring.h`; CLAIM-SPSC-001 latency claim depends on this. |

### 13.3 SHM Invariants (proved in S1)

| ID | Statement | Verification |
|---|---|---|
| **SHM-CONST** | All SHM region sizes are compile-time constants. No runtime region creation. | Static assertion in `shm_layout.h`; GC-E-010 Warning (Blocking at Phase 3). |
| **SHM-MLOCK** | All SHM regions are `mlock(2)`-ed at startup. Page faults on the hot path are impossible. | `mlock()` return value checked at startup; failure is a fatal startup error. |
| **SHM-NUMA** | All SHM regions are allocated NUMA-local to execution CPU socket. | `mbind()` / `numactl` configuration in startup sequence; verified by timing benchmark showing no NUMA crossing latency. |

---

## 14. Interfaces

This section defines the key interface contracts S1 must establish. Interface examples are illustrative; production C++ code is not prescribed here.

### 14.1 WalWriter Interface Contract

`engine/core/wal_writer.h` must expose a thread-safe, io_uring-backed writer satisfying:

- `write_sync(WalRecord*)` — writes and fsyncs one record; blocks until O_DSYNC returns; returns only after data is on stable storage
- `submit_and_wait()` must assert thread identity (WAL writer thread only) — via `ASSERT_WAL_THREAD()` macro
- Return value verification: must verify `write()` return equals buffer length; short writes = fatal error, not retryable
- CRC32C computation: must compute and write `header_crc32c` on every record before submission to io_uring
- Sequence monotonicity: internal sequence counter is `uint64_t`; incremented before each write; never resets

### 14.2 SpscRing Interface Contract

`engine/core/spsc_ring.h` must expose a templated ring satisfying:

- `push(const T& item)` — called from producer thread only; spins on full; `seq_cst` store on producer index after slot write
- `pop(T& item) -> bool` — called from consumer thread only; returns `false` if empty; `relaxed` load on producer index check
- Ring size: power-of-2 template parameter; modulo via bitmask
- Slot size: `sizeof(T)` — fixed at compile time; static assertion that T is trivially copyable
- Cache-line alignment: `alignas(64)` on producer and consumer index fields

### 14.3 ShmLayout Constants Interface

`engine/core/shm_layout.h` must expose `constexpr` constants following this pattern:

```cpp
namespace egbos::shm {
  inline constexpr std::size_t kMarketEventsSlots    = 65536;
  inline constexpr std::size_t kMarketEventsSlotSize = 128;   // bytes
  inline constexpr std::size_t kExecCommandsSlots    = 4096;
  inline constexpr std::size_t kExecCommandsSlotSize = 64;    // bytes
  // ... all 8 regions
  inline constexpr const char* kMarketEventsPath     = "/dev/shm/egbos_market_events";
  // ... all 8 paths
}
```

Changes to these constants require recompile and re-measurement of affected timing constants, and an ADR amendment to AD-006.

### 14.4 CrossPlane Interface Contract

`engine/core/cross_plane.h` must export:

- `CROSS_PLANE_READ(atomic_var, layer_from, layer_to)` macro — static assertion that `sizeof(atomic_var) == sizeof(int64_t)`; `acquire` memory order; compile-time layer documentation
- `LAYER_L3`, `LAYER_L4`, etc. as named constants for the macro parameters (enabling static analysis tooling to verify layer relationships)

---

## 15. Test Requirements

All tests must pass under ASAN + UBSAN (P3) and TSAN (P4). Tests against synthetic fixtures only; no live exchange data in S1.

### 15.1 WAL Tests (P3 + P4)

| Test | Description | Pass Criterion |
|---|---|---|
| `wal_write_and_read` | Write N records; read back; verify CRC32C and sequence on each | All records pass CRC; sequence is strictly monotonic |
| `wal_kill9_survival` | Write M records; inject SIGKILL at random WAL offsets; restart; replay | 10/10 trials (PROC-KILL9-001): recovered state byte-identical to pre-crash state minus at most 1 in-flight record |
| `wal_truncation_on_corrupt` | Corrupt one record's `header_crc32c` byte; restart; replay | Recovery stops at corrupted record; subsequent records correctly discarded |
| `wal_idempotent_replay` | Write WAL; replay 3× from same checkpoint | 3/3 replays produce byte-identical position state (PROC-REPLAY-001) |
| `wal_short_write_fatal` | Mock `write()` to return < buffer_length | Fatal error path invoked; TERMINAL_HALT signalled |
| `wal_thread_safety` (TSAN) | Two threads attempt concurrent CQE consumption | TSAN detects race; thread identity assertion fires |
| `wal_sequence_never_reset` | Restart WAL writer 100× from clean shutdown | Sequence continues from last-seen value; never resets to 0 |
| `wal_fsync_latency` (P5) | Measure WAL fsync P99 over 10,000 writes | Within 10% of `timing_constants.json` P99 baseline |

### 15.2 SPSC Tests (P3 + P4)

| Test | Description | Pass Criterion |
|---|---|---|
| `spsc_push_pop_basic` | Single producer/consumer; N items | All items received in FIFO order; no corruption |
| `spsc_full_spin` | Fill ring to capacity; verify no drop | Producer spins; zero events dropped; ring drains cleanly |
| `spsc_tsan_race_detection` (TSAN) | Two producer threads; one consumer | TSAN detects race — confirms single-producer constraint is necessary |
| `spsc_latency_p99` (P5) | 1M push/pop cycles; measure latency | P99 ≤ 50 ns against `timing_constants.json` baseline |
| `spsc_cache_line_isolation` | Static assertion + hardware counter verification | Static assertion passes; zero false-sharing detected under perf |
| `spsc_never_drops` | Stress 10× nominal throughput (650K ev/s × 60 s) | Zero dropped events; zero TSAN findings |

### 15.3 Pool Allocator Tests (P3 + ASAN)

| Test | Description | Pass Criterion |
|---|---|---|
| `pool_alloc_asan` | Run pool allocator through 1M allocate/free cycles | ASAN: zero `malloc`/`new` calls from pool paths |
| `pool_exhaustion_fatal` | Exhaust pool at startup | Fatal error path; startup aborts cleanly |
| `pool_no_double_free` | Double-return of pool slot | ASAN or assertion: double-free detected and handled |

### 15.4 Governance Fixture Tests (P1)

`scripts/verify_all_fixtures.sh` must verify each violation fixture causes its rule to fire. Passing a violation fixture = rule broken = CI exit 1.

| Fixture | Rule it must trigger |
|---|---|
| `violation_GC_E_010.cpp` | GC-E-010 (malloc in engine/) |
| `violation_GC_E_011.cpp` | GC-E-011 (CLOCK_REALTIME in engine/) — pre-registered for Phase 2 |
| `violation_GC_WAL_001.cpp` | GC-WAL-001 (state mutation before WAL fsync) |
| `violation_GC_E_053.cpp` | GC-E-053 (V3+ claim amended without RFC) |
| `violation_GC_E_054.cpp` | GC-E-054 (research SIMD flags non-compliant) |
| `violation_GC_E_SECRETS_001.sh` | GC-E-SECRETS-001 (secret in tracked file) |
| `violation_GC_E_056.js` | GC-E-056 (SIM_STATE accesses production SHM) — JavaScript fixture per HTML §B1.4.3 |
| `violation_GC_E_044.cpp` | GC-E-044 (memory_order_relaxed on cross-plane atomic) — pre-registered for Phase 4 |
| `violation_GC_E_045.cpp` | GC-E-045 (direct x_l_/order_tag_ read from L4/L5) — pre-registered for Phase 4 |

---

## 16. Verification Classes

*Authoritative source: HTML §B1.2.2 — Verification Classification System*

EGBOS uses the ENGINE_V3 verification taxonomy V1–V4 as the operational standard. V5/V6 are aspirational extensions for post-Phase 7 TLA+ work and are out of scope for S1.

| Class | Name | Meaning |
|---|---|---|
| **V1** | Manual Review | Property asserted based on human review. No automated enforcement exists. |
| **V2** | Compile-Time or Build-Time Enforcement | Violation produces compile-time error or build-system failure on all declared toolchain versions. Import-linter running at CI time (P1) is classified V2. |
| **V3** | Static Analysis Enforcement | Violation detected by named static analysis tool with named rule on all in-scope code paths. Fixture test requirement: must include violation fixture whose CI check verifies the fixture *fails* the rule. |
| **V4** | Empirical Validation | Property demonstrated under explicitly specified test conditions (hardware_id, kernel, duration, load). PROC-KILL9-001: kill-9 crash recovery requires 10 consecutive successful trials. |

**S1 claim targets:**

| Claim | Target Class | Minimum Class for Unblocked Deploy |
|---|---|---|
| CLAIM-WAL-001 | **V4** | V4 |
| CLAIM-SPSC-001 | **V4** | V4 |
| CLAIM-L3-POOL-001 | **V2** | V2 |

**Composed verification class:** When S3 claims (e.g., CLAIM-L3-001) depend on CLAIM-WAL-001, the composed class is `min(V4, V4) = V4`. Govcheck recomputes `composed_verification_class` automatically; the human-set value in YAML is checked but not trusted (GC-E-017).

---

## 17. Exit Criteria (Acceptance Gate)

*Authoritative source: HTML §I2 slice-S1*

The S1 acceptance gate command is:

```bash
govcheck validate --scope C0,P3,P4,P5 --strict
```

This command must return **exit code 0** with **0 errors and 0 warnings**.

### 17.1 Pass Criteria

All of the following must be true simultaneously:

1. **CLAIM-WAL-001 at V4:**
   - 10× kill-9 survival (PROC-KILL9-001): all 10 trials pass; recovered state byte-identical to pre-crash state
   - WAL fsync P99 ≤ 8,100 ns on the target hardware used for S1 testing
   - Sequence monotonicity across all recovery boundaries
   - CRC32C truncation correctly locates corruption boundary in all injection tests
   - Proof artifacts committed to `governance/proof_artifacts/wal_kill9_test_results/` with SHA-256 in claim

2. **CLAIM-SPSC-001 at V4:**
   - TSAN-clean: full test suite runs under TSAN with zero data races
   - P99 push/pop latency ≤ 50 ns against `timing_constants.json` baseline
   - Producer/consumer cache-line isolation statically asserted and verified
   - Proof artifacts committed

3. **CLAIM-L3-POOL-001 at V2:**
   - Code review sign-off documented in `review_history` field
   - ASAN run over pool allocator test suite: zero `malloc`/`new` calls from pool operation paths

4. **Zero TSAN findings** (P4 gate passing on all `engine/` PRs)

5. **P5 latency regression**: WAL fsync P99 and SPSC P99 within 10% of the committed `timing_constants.json` baseline

6. **CI gates active:** P3 (unit + integration), P4 (TSAN), P5 (latency regression) — all active and blocking

7. **GC-WAL-001 active and blocking:** Confirmed by fixture test `violation_GC_WAL_001.cpp` firing correctly

8. **GC-E-SECRETS-001/002 active:** No secret material in any tracked file; fixture fires correctly

9. **GC-E-056 active:** `SIM_STATE` production-SHM access fixture (`violation_GC_E_056.js`) fires correctly

---

## 18. Entry Conditions

*Authoritative source: HTML §I2 slice-S1*

S1 may not open until ALL of the following are true:

1. **S0 acceptance gate has passed:** `govcheck validate --scope C0 --strict` returned exit 0 on the S0 final state

2. **govcheck C0 gate is clean:** No governance errors or warnings in the current state of `governance/`

3. **CLAIM-000 at V1:** The bootstrap claim is active and at V1 verification class

The entry condition is binary: either S0 is govcheck-clean and CLAIM-000 is active at V1, or S1 does not open. There is no partial entry.

---

## 19. Failure Mode Specifications

*Rationale for this section: HTML §P-005 — Failure Mode Specification Precedes Nominal Specification*

### 19.1 WAL Failure Modes

| Failure | Detection Mechanism | Response |
|---|---|---|
| **WAL CRC corruption** (partial write, bit flip) | CRC32C check on recovery | Truncate at corrupted record; replay only valid prefix; Step 4 of recovery protocol |
| **WAL sequence gap** (partial write, device error) | Sequence monotonicity check | Same as CRC corruption: truncation point at gap; treated identically |
| **WAL device I/O failure** (O_DSYNC returns error) | `write()` return value check | TERMINAL_HALT — P0 incident (P-006: capital safety over continuity) |
| **WAL short write** (`write()` returns < buffer_length) | Return value check | Fatal error — not retryable; TERMINAL_HALT |
| **WAL shadow mirror lag > 100ms** | Health monitor on async mirror offset | P1 alert; operational degradation, not P0 |
| **Concurrent CQE consumer detected** (io_uring) | Thread identity assertion | TERMINAL_HALT; R4 remediation required before restart |
| **WAL writer pool exhaustion at startup** | Pool exhaustion check | Fatal startup error; engine does not enter operational loop |

### 19.2 SPSC Failure Modes

| Failure | Detection Mechanism | Response |
|---|---|---|
| **Ring full (sustained backpressure)** | Producer spin + utilisation counter | P1 alert at > 80% sustained utilisation; spin — never drop |
| **Ring reader crash** (consumer process dies) | Health monitor / heartbeat | Governance flag `halt_required = true`; producer observes and halts |
| **Second producer detected** (TSAN race) | TSAN + thread identity assertion | TSAN: CI failure (P4 hard block). Runtime: TERMINAL_HALT |
| **False sharing detected** | Static assertion failure | Compile-time error — prevents the build from completing |

### 19.3 SHM Failure Modes

| Failure | Detection Mechanism | Response |
|---|---|---|
| **`shm_open` failure at startup** | Return value check | Fatal startup error |
| **`mlock` failure** | Return value check | Fatal startup error (no page faults permitted on hot path) |
| **NUMA allocation failure** | `mbind()` return value check | Fatal startup error with NUMA topology diagnostic |
| **Runtime SHM region creation attempted** | Structurally impossible (compile-time constants) | Static assertion at compile time |

---

## 20. Exit Gates for S1

This section consolidates all measurable exit criteria into a binary checklist. S1 is closed if and only if every item is checked.

### 20.1 Governance Exit Gates

- [ ] `govcheck validate --scope C0,P3,P4,P5 --strict` returns exit 0, 0 errors, 0 warnings
- [ ] CLAIM-WAL-001 `status: "active"`, `verification_class: "V4"`, proof artifacts SHA-256 committed
- [ ] CLAIM-SPSC-001 `status: "active"`, `verification_class: "V4"`, proof artifacts SHA-256 committed
- [ ] CLAIM-L3-POOL-001 `status: "active"`, `verification_class: "V2"`, `review_history` entry present
- [ ] All three S1 claims have `owner` present in `governance/team.yaml`
- [ ] GC-WAL-001 fixture `violation_GC_WAL_001.cpp` verified firing in P1
- [ ] GC-E-SECRETS-001/002 fixtures verified firing
- [ ] GC-E-056 fixture `violation_GC_E_056.js` verified firing in P1

### 20.2 Empirical Exit Gates

- [ ] 10/10 WAL kill-9 trials (PROC-KILL9-001): recovered state byte-identical to pre-crash state
- [ ] WAL fsync P99 ≤ 8,100 ns on S1 target hardware (documented in proof artifact with `hardware_id`)
- [ ] SPSC push/pop P99 ≤ 50 ns on S1 target hardware (documented in proof artifact)
- [ ] Zero TSAN findings on full `engine/` test suite
- [ ] WAL P99 and SPSC P99 within 10% of committed `timing_constants.json` baseline (P5 gate passing)

### 20.3 CI Gate Exit Gates

- [ ] P3 (Unit + Integration): active and blocking on all PRs; zero test failures
- [ ] P4 (TSAN): active and blocking on all `engine/` PRs; zero races
- [ ] P5 (Latency Regression): active and blocking on hot-path PRs; baseline committed

### 20.4 Forbidden Action Verification Gates

- [ ] No `malloc`/`new`/`free`/`delete` in `engine/` hot-path code (GC-E-010 Warning; fixture verified)
- [ ] No `CLOCK_REALTIME` in `engine/` code (GC-E-011 pre-registered; fixture verified)
- [ ] No runtime SHM region sizing (AD-006; verified by static assertion in `shm_layout.h`)
- [ ] No second CQE consumer thread on WAL writer io_uring ring (AD-005; TSAN + assertion)
- [ ] No secret material in tracked source files (GC-E-SECRETS-001; fixture verified)

### 20.5 Capital Gate

**Capital Gate: None.** S1 carries no capital gate. No real, staging, or paper capital deployment authority is unlocked by S1 closure. S1 closure authorises opening S2.

---

## Appendix A — Claim Dependency Graph

```
CLAIM-000 (S0, bootstrap, V1)
  └─ CLAIM-WAL-001 (S1, wal.durability, V4)         ← S1 proves
       └─ CLAIM-L3-001 (S3, exec.wal_ordering, V4)  ← S3 proves; depends on WAL-001
            └─ CLAIM-INV-001 (S3, exec.position, V4) ← S3 proves; depends on L3-001

CLAIM-000 (S0, bootstrap, V1)
  └─ CLAIM-SPSC-001 (S1, ipc.spsc, V4)              ← S1 proves
       └─ CLAIM-L3-004 (S3, exec.memory_ordering, V4) ← S3 proves; depends on SPSC-001
       └─ CLAIM-L3-006 (S3, exec.atomic_exposure, V4) ← S3 proves; depends on SPSC-001

CLAIM-000 (S0, bootstrap, V1)
  └─ CLAIM-L3-POOL-001 (S1, allocator.pool, V2)     ← S1 proves
```

Namespace cross-references (HTML §B1.8): `exec.*` → `wal.*`, `ipc.*`, `allocator.*`. Undeclared cross-namespace dependencies trigger GC-E-023.

---

## Appendix B — Timing Reference

These targets apply during S1 testing before production hardware is available. They are superseded by empirical measurements in S6.

| Measurement | S1 Acceptance Criterion | S6 Production Baseline Source |
|---|---|---|
| WAL fsync P99 | ≤ 8,100 ns (CLAIM-WAL-001 V4 target) | `timing_constants.json::wal_fsync_p99_ns` |
| SPSC push/pop P99 | ≤ 50 ns (CLAIM-SPSC-001 V4 target) | `timing_constants.json::spsc_push_pop_p99_ns` |

The authoritative example production values from the HTML §C5.2 `timing_constants.json` schema (measured on specific production hardware — not S1 targets):

```json
{
  "schema_version": 1,
  "hardware_id": "colo-rack-A-server-01",
  "measurements": {
    "wal_fsync_p50_ns":          4200,
    "wal_fsync_p99_ns":          8100,
    "wal_fsync_p999_ns":         18400,
    "spsc_push_pop_p50_ns":      42,
    "spsc_push_pop_p99_ns":      180,
    "nvme_write_throughput_mbps": 3100
  }
}
```

> **Note:** The SPSC P99 acceptance criterion for CLAIM-SPSC-001 is ≤ 50 ns (HTML §I2 slice-S1). The example `timing_constants.json` shows `spsc_push_pop_p99_ns: 180` for a specific production hardware profile. Both values are from the HTML. The claim target (≤ 50 ns) and the example production measurement (180 ns) are independent data points; the claim target governs S1 acceptance.

---

## Appendix C — Governance Conflict Resolution

This document is derived exclusively from `EGBOS_MASTER_augmented.html` (Freeze Authority Edition v1.0, Certification ID: EGBOS-FREEZE-2026-001). In the event of any conflict between:

- This document and the HTML → **the HTML prevails unconditionally**
- This document and a previous informal S1 description (chat, notes, generated file) → **this document prevails**
- This document and AUDIT_PROMPT_S0.md → **this document prevails for S1 scope; AUDIT_PROMPT_S0.md remains authoritative for S0 scope**

Changes to this document after S1 has opened require a formal RFC (HTML §I6).

---

*Document ID: EGBOS-SPEC-S1-001 | Source: EGBOS_MASTER_augmented.html v1.0 | Conflict Rule: HTML supersedes all*
