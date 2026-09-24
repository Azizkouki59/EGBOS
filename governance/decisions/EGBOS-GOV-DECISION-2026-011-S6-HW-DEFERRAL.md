# GOVERNANCE BOARD DECISION — S6 HARDWARE GATE DEFERRAL

Decision Authority: Governance Board (Founder-Solo, per A2.5)
Decision Reference: EGBOS-GOV-DECISION-2026-011
Decision Timestamp: 2026-09-24T12:37:07+01:00
Approval Status: SIGNED

---

## Decision ID: GOV-S6-HW-DEFERRAL-001

## 1. Subject

Formal deferral of S6 hardware procurement gates (PROC-001 through PROC-004) pending
physical hardware acquisition.

## 2. Background

Slice S6 (Production Hardware Qualification) has two parallel workstreams as documented
in docs/S6_INSTITUTIONAL_SPEC.md §1:

| Workstream | Status |
|---|---|
| SW — Software defect remediation | SUBSTANTIALLY COMPLETE (pending tree cleanup + commit) |
| HW — Physical production hardware procurement | DEFERRED — hardware not yet procured |

The current development host (aziz-Thin-15-B13UDX, Intel i5-13420H) does not satisfy
and is not claimed to satisfy any PROC-001 through PROC-004 requirement. This decision
does NOT relabel the development host as production-qualified.

## 3. Formal Findings

### Finding F-001: Software Workstream — Substantially Complete

| Req | Description | State |
|---|---|---|
| S6-REQ-001 | LOB arithmetic overflow safety (UBSan/libFuzzer) | IMPLEMENTED — needs commit |
| S6-REQ-002 | Parser-level qty bounds rejection | IMPLEMENTED — needs commit |
| S6-REQ-003 | CTest suite green (220/220, 2 credential-gated skips) | PASS |
| S6-REQ-004 | S5 market-data boundary script corrected | IMPLEMENTED — needs commit |

Evidence: governance/proof_artifacts/s6_price_to_index_ubsan_evidence.md (committed 6a498e9)

### Finding F-002: Hardware Workstream — Deferred

| Gate | Required | Current | Status |
|---|---|---|---|
| PROC-001 | Tier-3/4 colocation; ≤2ms latency to Binance | Developer laptop | NOT SATISFIED |
| PROC-002 | AMD EPYC 9004 / Xeon Sapphire Rapids; ≥32 cores | Intel i5-13420H; 8 cores | NOT SATISFIED |
| PROC-003 | PCIe Gen4 NVMe; ≥2TB dedicated; P99 fsync <100µs | 477GiB shared NVMe | NOT SATISFIED |
| PROC-004 | 10GbE; DPDK/RDMA kernel bypass; <5µs P99 loopback | Wi-Fi only | NOT SATISFIED |

These requirements are UNCHANGED. This decision does not modify, weaken, or reinterpret them.

### Finding F-003: S6 Gate Remains OPEN

The S6 exit gate (governance/exit-gates/SLICE-S6-exit-v1.yaml) MUST remain
status: OPEN until all 6 acceptance criteria — including all PROC gate records
countersigned — are satisfied. This decision does not change that gate status.

### Finding F-004: Paper Trading Is Not Authorized

EGBOS_MASTER.md §I2 MS-005 (as amended by RFC-001) requires the S6 exit-gate signed
before paper trading is authorized. This decision does not authorize paper trading;
the S6 exit-gate remains OPEN and no 48h staging run has been recorded.

### Finding F-005: Real Capital Prohibited

Real capital deployment remains prohibited under all conditions.

## 4. Decision

### D-001: PROC Deferral Formally Recorded

PROC-001 through PROC-004 are formally recorded as DEFERRED pending hardware
procurement. Their requirements remain unchanged and must be satisfied in full before
S6 can close.

### D-002: Software Workstream Formally Acknowledged

The S6 software workstream is acknowledged as substantially complete. The
software-side engineering work (L2, L3, L4, paper execution path, working tree
cleanup) may proceed on the development host as legitimate engineering progress.
This does not constitute production operation or paper trading authorization.

### D-003: Prohibited Actions

The following actions remain PROHIBITED regardless of software implementation state:

1. Submitting any real order to Binance or any exchange
2. Using real exchange API trading credentials
3. Deploying any capital (real or staged)
4. Claiming S6 is closed or paper trading is authorized
5. Relabeling development timing measurements as production evidence
6. Reporting the current development host as production-qualified
7. Initiating the 48-hour staging clock on the development host

### D-004: Engineering Work Authorized

The following activities are authorized to continue on the development host:

1. L2 implementation (book-derived state from LobEngine)
2. L3 implementation (risk controls, position tracking, exposure safety)
3. L4 implementation (execution safety, halt conditions, fail-closed paths)
4. Paper execution path (simulated fills only — zero real orders)
5. Working tree cleanup (commit S6-REQ-001/002/004, gitignore build artifacts)
6. End-to-end pipeline test using real Binance MARKET DATA only
   (Binance → L0 → L1 → LOB → L2 → L3 → L4 → paper fill — market data read path only)
7. Test authoring and validation

### D-005: PROC Re-Activation Conditions

Deferral for each PROC gate is lifted when:
- Physical hardware is procured
- Required benchmarks are run and recorded
- Evidence artifacts committed to governance/proof_artifacts/
- PROC YAML countersigned by Founder
- TIMING-CONSTANTS-001.yaml updated and signed
- S6 acceptance criterion verified and marked SATISFIED

## 5. Required Follow-Up to Close S6

| Action | Owner | Gate |
|---|---|---|
| Commit S6-REQ-001/002/004 software changes | Engineering | S6 software |
| Procure colocation; countersign PROC-001 | Founder | S6-AC-001 |
| Procure production server; countersign PROC-002 | Founder | S6-AC-002 |
| Procure ≥2TB dedicated NVMe; run fio; countersign PROC-003 | Founder | S6-AC-003 |
| Procure 10GbE NIC; configure DPDK; countersign PROC-004 | Founder | S6-AC-004 |
| Sign LIVE-CAPITAL-001.yaml | Founder | S6-AC-005 |
| Sign TIMING-CONSTANTS-001.yaml (after prod benchmarks) | Founder | S6-AC-006 |
| Close SLICE-S6-exit-v1.yaml | Engineering + Founder | S6 gate |
| Enter and close S7 | Engineering + Founder | S7 gate |
| Paper trading then authorized (MS-005) | — | — |

## 6. Governance Notes

- This decision is append-only and cannot be retroactively modified
- This decision does NOT constitute S6 gate closure
- This decision does NOT authorize paper trading
- This decision does NOT weaken any PROC requirement
- SLICE-S6-exit-v1.yaml remains status: OPEN

---

## Signature

```yaml
signer: aziz
date: 2026-09-24T12:37:07+01:00
git_commit_sha: a1158cd061be7c170233f4c778823b325f954842
```

This document has governance force as of the commit identified above.
Amended in the same commit via `git commit --amend --no-edit` to populate this
signature block; the amend SHA supersedes the initial a1158cd SHA for record purposes.
