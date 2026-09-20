# S5 STEP 18 GOVERNANCE DECISION PACKAGE

## GOV-S5-18-A — REQ-001 Acceptance Criterion

**EVIDENCE:** 
Implementation of `EGBOS-GOV-DECISION-2663389-S5-16` Option A (rate-calibrated workload of 500 µs mean inter-operation interval with ±50 µs jitter) resulted in `HWM = 4096 / 4096 = 100%`. The `<90%` criterion failed.

The forensic audit confirms that under immutable `SpscRing` semantics, `tail_cache_` is refreshed exclusively when `head_ - tail_cache_ >= Capacity`. Because `tail_cache_` begins at 0, the first 4096 pushes will monotonically drive `head_` to 4096 while `tail_cache_` remains 0. Consequently, the approved metric `PRODUCER_SIDE_UPPER_BOUND = head_ - tail_cache_` will *always* reach exactly 100% (4096) for any workload that pushes ≥ 4096 messages over its lifetime, regardless of rate calibration.

**REQUIRED DECISION:**
The Governance Board must explicitly choose ONE of the following options. Do NOT implement either option without formal approval.

### Option B1 — Reformulate the acceptance criterion
Keep the approved metric:
`PRODUCER_SIDE_UPPER_BOUND = head_ - tail_cache_`
but replace the `<90%` criterion with a criterion compatible with the metric's lazy refresh semantics. 
**Threshold:** `BOARD_INPUT_REQUIRED`

OR

### Option B2 — Reject the approved metric
Return REQ-001 to governance and select a different cache-isolation-preserving metric.

---

## GOV-S5-18-B — CLAIM-L0-002

**EVIDENCE:**
The S5 STEP 18 forensic audit identified that `governance/claims/CLAIM-L0-002.yaml` contains unauthorized changes in the working tree (trailing whitespaces, modified `class_rationale`, and modified `previous_class` to `V3`). 

**REQUIRED DECISION:**
Governance must explicitly determine whether:
1. The entire claim should be restored byte-for-byte to HEAD; OR
2. Specific pre-existing changes are legitimate and require a separate authorization record.

No claim promotion or reclassification is permitted without this decision.

---

## REQ-004

**RECORD:**
* The approved REQ-004 contract remains authoritative.
* Owner-side snapshot APIs were authorized by `EGBOS-GOV-DECISION-2663389-S5-16`.
* Implementation must remain halted only where verification/architecture requires it.
* No new REQ-004 governance amendment should be invented unless an actual contract contradiction is demonstrated.
