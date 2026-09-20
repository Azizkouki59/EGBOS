# GOVERNANCE BOARD DECISION — S5 STEP 21

Decision Authority: Governance Board
Decision Reference: EGBOS-GOV-DECISION-2663389-S5-21
Decision Timestamp: 2026-08-18T23:29:16Z

## GOV-S5-18-A

**REQ-001 CRITERION: Candidate 3** (Refresh Frequency Limit / Backpressure Rate)

**THRESHOLD:** `Total Lazy Refreshes <= (Total Pushes / Capacity) * 1.05`

*Rationale:* 
Candidate 3 measures the total number of times `tail_cache_` is forced to refresh via `std::memory_order_acquire`. Under the immutable lazy-refresh semantics of `SpscRing`, the theoretical minimum number of refreshes for a perfectly fast consumer is exactly `Total Pushes / Capacity`. A threshold of 5% above this theoretical minimum permits minor operational jitter but strictly prohibits sustained backpressure that would cause the number of refreshes to spike. This metric accurately models `PRODUCER_SIDE_UPPER_BOUND` headroom without violating cache isolation.
