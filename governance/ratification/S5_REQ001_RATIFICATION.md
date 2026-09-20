# S5-REQ-001 — Execution Ring HWM Formal Ratification Package

**Document status:** APPROVED
**Prepared by:** Implementation Agent (S5 STEP 10)
**Spec reference:** S5-REQ-001 / `docs/S5_INSTITUTIONAL_SPEC.md` line 51–60
**Claim:** `CLAIM-L3-05`

---

## 1. Requirement

**Original:** `EXACT_INSTANTANEOUS_HWM`

S5-REQ-001 requires that the startup buffer (`egbos_exec_commands`, SPSC ring,
4096 × 64-byte slots) be sized empirically against live feed jitter. The
original formulation implied reporting the exact instantaneous queue depth
(`head - tail`) as the measurement vehicle.

---

## 2. Architectural Finding

### SPSC Cache-Line Ownership

`SpscRing<T, Capacity>` enforces strict lock-free cache-isolation:

| Cache line | Atomic variable | Non-atomic cached copy | Exclusive owner |
|---|---|---|---|
| Cacheline 1 | `head_` (Producer index) | `tail_cache_` (stale consumer view) | **Producer thread only** |
| Cacheline 2 | `tail_` (Consumer index) | `head_cache_` (stale producer view) | **Consumer thread only** |

### Why Exact HWM Is Impossible Without Synchronization

An exact instantaneous queue depth requires simultaneously observing:

```
exact_depth = head_ - tail_
```

where `head_` (Cacheline 1) and `tail_` (Cacheline 2) are owned by
**different threads on different cache lines**.

Reading both indices in the same operation forces a cross-cacheline acquire,
which introduces:
- Cache coherence traffic on the hot path
- A de-facto synchronization point between producer and consumer
- A violation of the SPSC lock-free cache-isolation invariant

This is architecturally incompatible with the EGBOS zero-allocation,
zero-synchronization SPSC hot-path contract (GC-E-010, AD-006).

---

## 3. Proposed Governance Replacement

### Selected Metric: `PRODUCER_SIDE_UPPER_BOUND`

```cpp
// Measured exclusively by the Producer thread.
// No cross-cacheline read. No new atomic. No ABI change.
uint64_t hwm_upper_bound = head_ - tail_cache_;
```

### Ownership Properties

| Variable | Owner | Nature |
|---|---|---|
| `head_` | Producer | Relaxed read of its own index — zero cost |
| `tail_cache_` | Producer | Stale local copy of consumer index — already present on Producer cacheline |

- **No cross-cacheline access introduced.**
- **No new atomic operation introduced.**
- **No SHM ABI change required.**
- **No modification to `SpscRing`, `ExecutionRouter`, or `shm_layout.h` required** beyond the addition of a read-only accessor on the producer side.

### What the Metric Is

`UPPER_BOUND` — not the exact instantaneous queue depth.

### What the Metric Is NOT

- It is NOT the exact instantaneous queue depth.
- It is NOT equivalent to `head_ - tail_`.
- It MUST NOT be used to claim exact HWM measurement in any governance claim.
- It MUST NOT be used to claim zero command-drops without independent corroborating evidence.

---

## 4. Mathematical Property

Since `tail_cache_` is the Producer's stale cached view of the consumer index,
updated only on `push()` backpressure events, it satisfies:

```
tail_cache_ ≤ tail
```

(monotonically: the consumer index can only advance, and `tail_cache_` lags
behind or equals `tail_`)

Therefore:

```
head_ - tail_cache_ ≥ head_ - tail_
         ↑ upper bound        ↑ exact depth
```

The proposed metric is a **conservative overestimate** of the actual queue
occupancy. Under production conditions it may read higher than actual depth
(when `tail_cache_` has not been refreshed since the last backpressure spin).

---

## 5. Authorized Usage

The `PRODUCER_SIDE_UPPER_BOUND` metric may be used to:

- Confirm the buffer was not saturated (i.e., upper bound stayed well below
  4096 slots) under the measured load scenario.
- Serve as the empirical buffer-sizing evidence required by S5-REQ-001 with
  the explicit understanding that the reported figure is an upper bound.

The metric may NOT be used to:

- Claim exact instantaneous queue depth.
- Assert zero command drops without independent WAL-level gap evidence.
- Promote `CLAIM-L3-05` beyond V3 without a separate governance motion.

---

## 6. Evidence Artifact Required

Once authorized, the following evidence artifact must be produced:

- **Artifact path:** `governance/proof_artifacts/s5_req001_hwm_upper_bound.json`
- **Content:** Observed `PRODUCER_SIDE_UPPER_BOUND` HWM value and percentage of ring
  capacity under the live jitter measurement scenario.
- **Required result:** `HWM_upper_bound < 90% × 4096 = 3686 slots` under peak
  exchange jitter load.

---

## 7. Required Claim Amendment (Post-Approval Only)

After governance approval, `CLAIM-L3-05.yaml` must be amended to:

1. Update `class_rationale` to state that the HWM figure is a `PRODUCER_SIDE_UPPER_BOUND`.
2. Add proof artifact reference to `s5_req001_hwm_upper_bound.json`.

**Do NOT amend `CLAIM-L3-05.yaml` before explicit approval.**

---

## 8. Governance Decision

```
DECISION: APPROVED
```

```
[x] APPROVED
[ ] REJECTED
[ ] AMENDMENT REQUIRED

Authority: Governance Board

Name:      Governance Board

Date:      2026-08-18T16:30:38Z

Decision Reference: EGBOS-GOV-DECISION-2663389-S5-12
```
