# S5 STEP 21 — REQ-001 CRITERION DEFINITION PACKAGE

## 1. Context and Semantic Distinction

The approved metric for REQ-001 is:
`PRODUCER_SIDE_UPPER_BOUND = head_ - tail_cache_`

This metric must be clearly distinguished from both instantaneous occupancy and cumulative lifetime pushes:

1. **Instantaneous Occupancy (`head_ - tail_`)**: 
   The true number of items in the ring at an exact moment. Measuring this requires a cross-cacheline read (the producer reading the consumer's `tail_` directly on every push), which violates strict SPSC cache isolation and introduces severe hot-path latency.

2. **Approved Producer-Side Upper Bound (`head_ - tail_cache_`)**:
   The number of items the producer *believes* are in the ring based on its cached view of the consumer's progress. `tail_cache_` is updated lazily (only when the producer believes the ring is full, i.e., `head_ - tail_cache_ >= Capacity`). This preserves the zero-cross-cacheline hot-path invariant.

3. **Cumulative Lifetime Pushes**:
   Because `tail_cache_` begins at 0 and is not refreshed until `head_` reaches `Capacity` (4096), the first 4096 pushes will monotonically drive `head_ - tail_cache_` to 4096 regardless of how fast the consumer is actually emptying the ring. A cumulative-lifetime HWM of this metric will incorrectly record 100% capacity for *any* workload exceeding 4096 total pushes over its entire lifetime. 

Therefore, any valid acceptance criterion must measure `PRODUCER_SIDE_UPPER_BOUND` in a way that respects its lazy refresh semantics without incorrectly penalizing the initial (or any subsequent) bounded `tail_cache_` staleness.

---

## 2. Candidate Acceptance Criteria

The Governance Board must select ONE of the following candidates (or provide another explicitly justified criterion) and define its measurable threshold.

### Candidate 1: Steady-State Bounded Refresh Distance
**Definition:** After startup fill (e.g., after the first `Capacity` pushes), measure the difference between `head_` and the newly updated `tail_cache_` immediately *after* a lazy refresh occurs. 
**Measurement Window:** Excludes the first 4096 pushes. Measured only on the exact iterations where `tail_cache_` is explicitly refreshed via `std::memory_order_acquire`.
**Pass/Fail Rule:** The post-refresh bound (`head_ - new_tail_cache_`) must remain below the `[THRESHOLD]`.
**Compatibility:** Directly aligns with lazy refresh by only measuring at the exact point the producer synchronizes with the consumer.
**Invariant Preservation:** Relies strictly on the existing `SpscRing::push()` logic without introducing any new cross-cacheline reads.

### Candidate 2: Rolling Window Sampling
**Definition:** Sample `head_ - tail_cache_` periodically (e.g., every 100 milliseconds) over the duration of the test, and calculate the mean and 99th percentile (p99).
**Measurement Window:** The entire rate-calibrated execution duration, sampled asynchronously by a non-intrusive monitor or aggregated post-run.
**Pass/Fail Rule:** The p99 of sampled values must remain below the `[THRESHOLD]`.
**Compatibility:** Acknowledges that the metric will sawtooth between 0 and 4096, but ensures the statistical distribution indicates sufficient headroom. 
**Invariant Preservation:** Requires no changes to `SpscRing` as the sampling occurs outside the hot-path push/pop operations.

### Candidate 3: Refresh Frequency Limit (Backpressure Rate)
**Definition:** Measure the total number of times `tail_cache_` is forced to refresh (i.e., backpressure events) over a defined period (e.g., 60 seconds).
**Measurement Window:** Entire execution duration.
**Pass/Fail Rule:** The number of lazy refreshes must not exceed `[THRESHOLD]` during the rate-calibrated test.
**Compatibility:** Instead of measuring the *value* of the bound, it measures how often the producer is forced to sync with the consumer, directly quantifying backpressure without being skewed by the 4096 bound limit.
**Invariant Preservation:** Does not alter `SpscRing` semantics; only requires the harness to count the times the upper bound reaches capacity.

---

## 3. REQUIRED GOVERNANCE DECISION FORMAT

The Governance Board must explicitly select a criterion and provide a measurable threshold. Do not infer or invent the threshold.

```text
GOV-S5-18-A: REQ-001 CRITERION: <Candidate 1, 2, 3, or other>
THRESHOLD: <explicit measurable value or formula>

Decision Authority: Governance Board
Decision Reference: <unique reference>
Decision Timestamp: <current repository timestamp>
```

Until this information is supplied, implementation remains halted.
