# S5-REQ-001 — Execution Ring HWM Measurement Decision

**Governance Status:** DECISION REQUIRED
**Target Release:** V4 / S5

## 1. Background

S5-REQ-001 requires reporting the high-water mark (HWM) of the execution ring queue depth (`egbos_exec_commands`, 4096 slots) to guarantee safety sizing under production loads.

## 2. Blocked Implementation: Exact Instantaneous HWM

The requirement originally implied an exact instantaneous queue depth measurement (`head - tail`). 
This has been **REJECTED** and implementation is HALTED.

### Reason for Rejection

The `SpscRing` is built on a strict lock-free cache-isolation architecture:
- Producer only touches `tail_` and `head_cache_` (cacheline 1).
- Consumer only touches `head_` and `tail_cache_` (cacheline 2).

An exact instantaneous measurement requires reading the current real `head_` and the current real `tail_` at the same time. This forces a cross-cacheline atomic read that violates the isolation invariants of the SPSC queue, injecting contention into the lock-free hot path.

## 3. Available Alternatives for Governance Approval

To satisfy the safety requirement without violating SPSC cache isolation, governance must formally select one of the following proxy metrics.

### Option A: Producer-side LOWER_BOUND HWM
```cpp
// Measured by Producer (e.g. FeedIngestor)
uint64_t depth = head_cache_ - tail_;
```
- **Pros:** Zero additional cache misses (reads existing local state).
- **Cons:** Represents a lower bound of the actual queue depth, since the consumer may have advanced `head_` since the last cache refresh.

### Option B: Consumer-side UPPER_BOUND HWM
```cpp
// Measured by Consumer (e.g. ExecutionRouter)
uint64_t depth = head_ - tail_cache_;
```
- **Pros:** Zero additional cache misses (reads existing local state).
- **Cons:** Represents an upper bound of the actual queue depth (safe conservative estimate), since the producer may have advanced `tail_` since the last cache refresh.

### Option C: Out-of-band Polling
A separate thread occasionally reads both `head_` and `tail_` with relaxed atomics.
- **Pros:** Does not block the hot path.
- **Cons:** The measurement is approximate and could miss microsecond-level spikes.

## 4. Required Action

Governance must formally define the V4 semantics for S5-REQ-001 by choosing one of the alternatives above.
Implementation is structurally blocked until a decision is recorded.
