# S5-REQ-001 — Execution Ring HWM Governance Amendment

## 1. Reference
Amendment to S5-REQ-001 (Live Jitter Buffer Sizing) and `governance/S5_REQ001_HWM_DECISION.md`.

## 2. Incompatibility of Exact HWM
The original requirement for EXACT_INSTANTANEOUS_HWM (`head - tail`) forces a cross-cacheline atomic read. `SpscRing` is built on a strict lock-free cache-isolation architecture where the producer only mutates `tail_cache_` and `head_` (cacheline 1) and the consumer only mutates `head_cache_` and `tail_` (cacheline 2). An exact measurement injects synchronization contention into the zero-allocation hot path, violating the primary isolation invariant.

## 3. Metric Selection Ambiguity
**UNRESOLVED — GOVERNANCE INPUT REQUIRED**

The previous decision matrix incorrectly mapped producer/consumer variables:
- `UPPER_BOUND = head_ - tail_cache_` uses a stale tail, but it MUST be measured by the **Producer**, as the Producer owns both variables.
- `LOWER_BOUND = head_cache_ - tail_` uses a stale head, but it MUST be measured by the **Consumer**, as the Consumer owns both variables.

The previous guidance erroneously stated `head_ - tail_cache_` was "Consumer-side". Because of this fundamental architectural contradiction, the metric selection is STOPPED.

Governance must formally resolve the producer/consumer thread ownership associated with the requested bound before implementation can proceed. No modification to `CLAIM-L3-05.yaml`, `spsc_ring.h`, `execution_router.*`, or `shm_layout.h` is authorized.
