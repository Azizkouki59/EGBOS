# GOVERNANCE BOARD DECISION — S5 STEP 20

Decision Authority: Governance Board
Decision Reference: EGBOS-GOV-DECISION-2663389-S5-20
Decision Timestamp: 2026-08-18T23:25:35Z

## GOV-S5-18-A

**DECISION: B1 — APPROVED**

Retain:
`PRODUCER_SIDE_UPPER_BOUND = head_ - tail_cache_`

The existing `<90%` acceptance criterion is REJECTED because the forensic evidence demonstrates that it is incompatible with the approved lazy-refresh SPSC semantics.

**New acceptance criterion:**
The criterion must measure the producer-side upper-bound metric without requiring cumulative-lifetime HWM to remain below a fixed percentage of capacity.

The implementation must NOT modify SPSC cache-isolation semantics.

This criterion requires a subsequent explicit measurable threshold definition before final REQ-001 closure. REQ-001 remains BLOCKED for implementation/closure until that measurable criterion is formally recorded.

## GOV-S5-18-B

**DECISION: C1 — APPROVED**

Restore:
`governance/claims/CLAIM-L0-002.yaml`
byte-for-byte to `HEAD`.

No claim promotion, reclassification, or amendment is authorized by this decision.

## REQ-004

The previously ratified REQ-004 contract remains authoritative under:
`EGBOS-GOV-DECISION-2663389-S5-16`

No new contract changes are authorized.
