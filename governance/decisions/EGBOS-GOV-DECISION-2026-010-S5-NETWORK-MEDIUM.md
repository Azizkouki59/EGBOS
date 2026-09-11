# GOVERNANCE BOARD DECISION — S5 NETWORK MEDIUM AUTHORIZATION

---

```
Decision ID:      GOV-S5-NETWORK-01
Decision:         AUTHORIZE_WIFI_FOR_S5_QUALIFICATION
Status:           APPROVED
Decision Authority: Governance Board
Decision Reference: EGBOS-GOV-DECISION-2026-010
Decision Timestamp: 2026-09-11T15:11:00Z
Signer:           founder
```

---

## 1. Background and Governance Gap

### 1.1 Origin of the Gap

During S5-AC-001 preflight (identified circa 2026-09-09), the following governance
gap was formally documented in
`governance/decisions/S5_AC001_WIFI_QUALIFICATION_DECISION_PACKAGE.md`:

The authoritative acceptance documents — `SLICE-S5-exit-v1.yaml`,
`docs/S5_INSTITUTIONAL_SPEC.md`, `governance/MARKET-DATA-001.yaml`, and
`EGBOS_MASTER.md` — collectively require "empirical measurement on live data"
for S5 qualification activities but do not codify an acceptable network transport
medium (e.g., Ethernet vs. Wi-Fi) for those activities.

The production Tier-0 host is currently connected to the Binance production
WebSocket endpoint via a Wi-Fi interface (`wlo1`). No authoritative EGBOS
document prohibits this transport.

### 1.2 Prior Scoped Request

The existing decision package `S5_AC001_WIFI_QUALIFICATION_DECISION_PACKAGE.md`
requested authorization scoped to S5-AC-001 only (under decision option
`GOV-S5-36-A: A1 = AUTHORIZE_WIFI_FOR_AC001_ONLY`), with AC-003 and AC-004
explicitly excluded pending separate authorization.

### 1.3 Expanded Scope of This Decision

This decision (`GOV-S5-NETWORK-01`) supersedes and absorbs `GOV-S5-36-A` by
authorizing Wi-Fi for all network-dependent S5 qualification activities defined
in Section 4 below. If the Board approves this decision, the prior scoped
request (`GOV-S5-36-A`) is satisfied as a subset.

### 1.4 Architectural Finding

D5 (`EGBOS-GOV-DECISION-2026-009`, §D5) mandates a Linux Tier-0 production
platform. D9 requires explicit production feed provenance. Neither D5 nor any
other D1-D9 invariant specifies Ethernet as a mandatory transport medium.
Absence of a prohibition in the authoritative corpus does not constitute
authorization; hence the need for this explicit Board decision.

---

## 2. Scope

### 2.1 In-Scope Acceptance Criteria

| AC | Description | Network dependency |
|---|---|---|
| S5-AC-001 | CLAIM-L3-05 at V4 — empirical startup buffer jitter measurement on live data | Required (WebSocket to Binance production) |
| S5-AC-002 | CLAIM-L0-002 at V4 (or V3 with waiver) — gap detection on real feed | Required for live-gap observation portion |
| S5-AC-003 | 14 consecutive days (336 hours) uptime, zero INV-001 violations | Required (continuous production feed) |
| S5-AC-004 (live-data acquisition portion) | 14 nightly Tier-0 WAL replay validations — live data collection phase only | Required for live data ingestion portion |

### 2.2 Out-of-Scope Acceptance Criteria

| AC | Reason excluded |
|---|---|
| S5-AC-005 | CI/govcheck activity — no network medium question |
| S5-AC-006 | Documentation sign-off activity — no network medium question |
| S5-AC-004 (offline replay computation) | Replay computation itself is offline (Tier-0 WAL already acquired) — no network medium dependency for the byte-identical comparison step |

### 2.3 Scope Limitation

This decision authorizes the **network transport medium** only. It does not
modify, waive, relax, or reinterpret any acceptance criterion, failure criterion,
invariant, evidence classification rule, provenance requirement, or LIVE_OPERATIONAL
definition within any of the in-scope criteria above.

---

## 3. Explicit Wi-Fi Authorization

**AUTHORIZED:**

> Wi-Fi may be used as the network transport medium for S5 qualification
> activities listed in Section 2.1, provided that the Binance production
> WebSocket endpoint (`wss://stream.binance.com:9443/ws`) is genuinely reachable
> and all acceptance criteria, D1-D9 invariants, and evidence classification
> rules remain completely unchanged.

**Ethernet is NOT a mandatory prerequisite** for any in-scope S5 activity unless
an authoritative EGBOS document explicitly requires it. No such requirement has
been identified in any reviewed source document.

**Wi-Fi instability is NOT an exemption** from any failure criterion. Specifically:

- A Wi-Fi outage during S5-AC-003 that causes an INV-001 violation constitutes
  a qualification failure. The 336-hour trial must restart from hour zero.
- A Wi-Fi disruption during nightly WAL acquisition that prevents byte-identical
  replay output constitutes a replay failure for that night. The night does not
  count toward the 14-night threshold.
- Wi-Fi connectivity loss must be logged, not concealed.

---

## 4. Network-Dependent S5 Scope (Detail)

### S5-AC-001 — Live Jitter Buffer Sizing

Authorization: Wi-Fi may be used for the empirical Binance production WebSocket
jitter measurement. The artifact must reference:
- Feed source: `wss://stream.binance.com:9443/ws` (production)
- Network interface in use (e.g., `wlo1`)
- Classification: LIVE_OPERATIONAL (after genuine live collection and human review)

### S5-AC-002 — Gap Detection on Real Feeds

Authorization: Wi-Fi may be used for the live-gap observation portion. The gap
detection must operate against genuine production sequence numbers. Testnet
sequences do not satisfy this criterion.

### S5-AC-003 — 336-Hour Continuous Uptime Trial

Authorization: Wi-Fi may be used as the network transport for the 336-hour
continuous live-feed trial. The timer restarts from zero on any INV-001 violation,
regardless of cause (including Wi-Fi outage). No credit is carried across a
restart. The requirement remains: 336 consecutive hours with zero INV-001
violations.

### S5-AC-004 — Live Data Acquisition (Nightly WAL)

Authorization: Wi-Fi may be used for the Tier-0 WAL live-data acquisition phase
(i.e., the phase in which the system receives live Binance data that populates
the WAL). The subsequent offline byte-identical replay computation is independent
of network medium.

---

## 5. Boundary Conditions

| Condition | Ruling |
|---|---|
| Wi-Fi outage during 336h trial | Qualification failure — trial restarts from hour zero |
| Wi-Fi outage during nightly data acquisition | That night does not count toward 14-night threshold |
| Ethernet available but Wi-Fi in use | Permitted under this authorization |
| Ethernet explicitly required by an EGBOS document | Ethernet required (no such document found in reviewed corpus) |
| Testnet used with Wi-Fi | Still DEVELOPMENT_TEST — not LIVE_OPERATIONAL |
| Synthetic/offline data collected over Wi-Fi | Still LOCAL_SYNTHETIC or OFFLINE_REPLAY — classification unchanged |
| Wi-Fi outage concealed or log-edited | Absolute prohibition — invariant fabrication |
| Failed 336h trial restarted and claimed as continuous | Absolute prohibition |

---

## 6. D1-D9 Impact Analysis

This decision authorizes a **network transport medium** and does not alter any
architectural invariant. All D1-D9 decisions from `EGBOS-GOV-DECISION-2026-009`
(APPROVED 2026-08-23T09:04:00Z) remain entirely unchanged.

| Invariant | Ruling | Impact |
|---|---|---|
| **D1** — Multicast Failure Policy (`EXPLICIT_NON_OPERATIONAL_STATE`) | UNCHANGED | Wi-Fi does not affect multicast fault classification |
| **D2** — Listener Termination Synchronization (`std::atomic<bool>`) | UNCHANGED | Software invariant; transport-independent |
| **D3** — Socket Error Contract (EAGAIN/EWOULDBLOCK/EBADF/etc.) | UNCHANGED | Error classification applies regardless of transport medium |
| **D4** — Timestamp Contract (`CLOCK_MONOTONIC_RAW`, nanoseconds) | UNCHANGED | Kernel clock; transport-independent |
| **D5** — Platform Boundary (Linux Tier-0) | UNCHANGED | Linux Tier-0 is still required; Wi-Fi does not substitute for it |
| **D6** — Polling Model (`IO_URING`) | UNCHANGED | Software architecture; transport-independent |
| **D7** — Feed Source Architecture (`REQUIRE_PROVENANCE_ABSTRACTION`) | UNCHANGED | Separation of DevelopmentTestFeed vs. ProductionFeed is unchanged |
| **D8** — UDP Buffer Size (`SOURCE_DEPENDENT`) | UNCHANGED | Buffer sizing still dictated by feed specification, not transport |
| **D9** — Live Source Provenance (`REQUIRE_PROVENANCE_BINDING`) | UNCHANGED | LIVE_OPERATIONAL evidence must still be explicitly bound to production source; Wi-Fi transport does not substitute for or relax provenance binding |

**Finding: Zero weakening of D1-D9.**

---

## 7. S5 Impact Analysis

### 7.1 Per-Criterion Analysis

| Criterion | Requirement text (unchanged) | Wi-Fi impact |
|---|---|---|
| **S5-AC-001** | CLAIM-L3-05 at V4 — empirical jitter on live data | None — measurement methodology unchanged; only transport medium clarified |
| **S5-AC-002** | CLAIM-L0-002 at V4 (or V3 with waiver) | None — gap detection criterion unchanged |
| **S5-AC-003** | 336 consecutive hours, zero INV-001 | None — 336-hour requirement unchanged; Wi-Fi outage = failure |
| **S5-AC-004** | 14 consecutive nightly byte-identical replays | None — replay criterion unchanged; acquisition medium clarified |
| **S5-AC-005** | `govcheck deploy-gate --target production` green | Not in scope of this decision |
| **S5-AC-006** | MARKET-DATA-001.yaml exit sign-off completed | Not in scope of this decision |

**Finding: Zero weakening of any S5 acceptance criterion.**

### 7.2 S5 Gate Status (Unchanged)

```
S5-AC-001: NOT_STARTED
S5-AC-002: NOT_STARTED
S5-AC-003: NOT_STARTED
S5-AC-004: NOT_STARTED
S5-AC-005: NOT_STARTED
S5-AC-006: PENDING  (MARKET-DATA-001.yaml countersigned 2026-08-19)
SLICE-S5-exit-v1 gate: OPEN
```

Authorization itself does NOT mean S5 has passed or is in progress.

---

## 8. LIVE_OPERATIONAL Preservation

The following invariants from `governance/MARKET-DATA-001.yaml` and
`governance/schemas/s5_artifact_schema.yaml` are **completely unchanged** by
this decision:

| Invariant | Status |
|---|---|
| `LOCAL_SYNTHETIC != LIVE_OPERATIONAL` | UNCHANGED |
| `UNIT_TEST != LIVE_OPERATIONAL` | UNCHANGED |
| `OFFLINE_REPLAY != LIVE_OPERATIONAL` | UNCHANGED |
| `TEST DATA != PRODUCTION EVIDENCE` | UNCHANGED |
| `Testnet feed is DEVELOPMENT_TEST — never LIVE_OPERATIONAL` | UNCHANGED |
| Automatic promotion from LOCAL_SYNTHETIC to LIVE_OPERATIONAL | PROHIBITED (unchanged) |
| Human reviewer required to set `v4_satisfied: true` | UNCHANGED |
| `review_status: REVIEWED_ACCEPTED` required for LIVE_OPERATIONAL artifacts | UNCHANGED |

**Current LIVE_OPERATIONAL state:**

```
LIVE_OPERATIONAL          = NONE
REAL_MARKET_DATA_SOURCE   = NOT_PROVISIONED (qualification not yet run)
```

This decision does not constitute LIVE_OPERATIONAL evidence.
**LIVE_OPERATIONAL is NOT ESTABLISHED.**

---

## 9. Testnet Exclusion

From `governance/MARKET-DATA-001.yaml` (unchanged):

```yaml
testnet:
  endpoint_market_data: "wss://testnet.binance.vision/ws"
  provenance_class: "DEVELOPMENT_TEST"
  note: "Testnet data is LOCAL_SYNTHETIC and MUST NOT be promoted to LIVE_OPERATIONAL evidence."
```

This decision authorizes Wi-Fi use for **production endpoint** connections only.

| Transport + Endpoint | Classification | LIVE_OPERATIONAL eligible? |
|---|---|---|
| Wi-Fi + `wss://stream.binance.com:9443/ws` (production) | EXTERNAL_LIVE → LIVE_OPERATIONAL (after qualification) | Yes — only after genuine qualification and human review |
| Wi-Fi + `wss://testnet.binance.vision/ws` | DEVELOPMENT_TEST / LOCAL_SYNTHETIC | **No — ever** |
| Ethernet + Testnet | DEVELOPMENT_TEST / LOCAL_SYNTHETIC | **No — ever** |
| Any transport + synthetic/offline data | LOCAL_SYNTHETIC or OFFLINE_REPLAY | No (unless re-run on live feed with human re-sign) |

**Testnet remains DEVELOPMENT_TEST. This is unchanged and cannot be altered by this decision.**

---

## 10. Evidence Classification Preservation

From `governance/schemas/s5_artifact_schema.yaml` (unchanged):

The four artifact classification values — `LOCAL_SYNTHETIC`, `OFFLINE_REPLAY`,
`LIVE_OPERATIONAL`, `ARCHITECTURAL_ENFORCEMENT` — remain the only valid
classifications. Transport medium does not alter artifact classification.
An artifact produced using Wi-Fi transport from the production Binance endpoint
is classified by the same rules as one produced using Ethernet: source provenance,
not transport medium, determines classification.

Upgrade rules (unchanged):

- `LOCAL_SYNTHETIC → LIVE_OPERATIONAL`: requires re-running the harness on live
  feed and a human governance reviewer re-signing the artifact.
- `OFFLINE_REPLAY → LIVE_OPERATIONAL`: requires 14 consecutive nightly runs on
  live WAL from production Tier-0.
- Auto-promotion without re-running collection is **absolutely prohibited**.
- "Copying a LOCAL_SYNTHETIC artifact and relabelling it LIVE_OPERATIONAL" is
  **absolutely prohibited**.

---

## 11. Governance Authority and Amendment Implications

### 11.1 Authority for This Decision

This decision is within the scope of Governance Board authority as established
by the existing governance structure. The Board has previously exercised
equivalent authority in:

- `EGBOS-GOV-DECISION-2026-009` (D1-D9 architectural invariants — APPROVED)
- `EGBOS-GOV-DECISION-2026-008` (WAL retention policy — APPROVED)
- `EGBOS-GOV-DECISION-2663389-S5-32` (provenance and evidence policy framework)

### 11.2 Specification Amendment Status

**`governance/MARKET-DATA-001.yaml`:**

- Current status: COUNTERSIGNED (signer: founder, date: 2026-08-19,
  commit: 26633896424d1ceda3818087ac50a379bd2e8778).
- No amendment is **strictly required** for the Wi-Fi authorization.
- A **recommended** amendment would add a `network_medium` field under
  `l0_integration` to explicitly record the authorized transport for audit
  purposes.
- **Specification amendment pending separate authorization.**

**`docs/S5_INSTITUTIONAL_SPEC.md`:**

- Current status: FROZEN (version 1.0, effective 2026-07-25).
- No amendment is **strictly required**. Existing requirements are transport-
  neutral and this decision provides the required clarification.
- A **recommended** amendment would add a note to S5-REQ-003 and S5-REQ-004
  referencing this decision (GOV-S5-NETWORK-01).
- **Specification amendment pending separate authorization.**

**Summary:** Specification amendments are **RECOMMENDED but not required** to
proceed with qualification after Board approval. The canonical files have
**NOT been modified**.

### 11.3 Supersession

This decision satisfies and supersedes the pending `GOV-S5-36-A` option from
`S5_AC001_WIFI_QUALIFICATION_DECISION_PACKAGE.md` as a subset of the broader
authorization granted here.

---

## 12. Operational State

```
DECISION STATUS:          APPROVED
WIFI AUTHORIZATION:       EFFECTIVE
S5-AC-001 STATUS:         NOT_STARTED
S5-AC-002 STATUS:         NOT_STARTED
S5-AC-003 STATUS:         NOT_STARTED
S5-AC-004 STATUS:         NOT_STARTED
S5-AC-005 STATUS:         NOT_STARTED
S5-AC-006 STATUS:         PENDING (MARKET-DATA-001.yaml countersigned)
LIVE_OPERATIONAL:         NOT ESTABLISHED
REAL_MARKET_DATA_SOURCE:  NOT_PROVISIONED
V4 PROMOTION:             DEFERRED
QUALIFICATION RUN:        NONE
EVIDENCE GENERATED:       NONE
BINANCE ORDERS PLACED:    ZERO (S5 is read-only; zero-order-path enforced per S5-REQ-005)
```

No qualification activities have been executed. No evidence has been generated.
No Binance orders have been placed.

---

## 13. Required Board Metadata

```
GOV-S5-NETWORK-01:      AUTHORIZE_WIFI_FOR_S5_QUALIFICATION

Decision Authority:     Governance Board
Decision Reference:     EGBOS-GOV-DECISION-2026-010
Decision Timestamp:     2026-09-11T15:11:00Z
Signer:                 founder

GOV-S5-36-A disposition (from S5_AC001_WIFI_QUALIFICATION_DECISION_PACKAGE.md):
  GOV-S5-NETWORK-01 is APPROVED; GOV-S5-36-A is satisfied as a subset.

Specification amendment authorization:
  governance/MARKET-DATA-001.yaml:    [SEPARATE AUTHORIZATION REQUIRED if desired]
  docs/S5_INSTITUTIONAL_SPEC.md:      [SEPARATE AUTHORIZATION REQUIRED if desired]
```

---

## 14. References

| Document | Path | Role |
|---|---|---|
| S5 L0 Architectural Decisions (APPROVED 2026-08-23) | `governance/decisions/EGBOS-GOV-DECISION-2026-009-S5-L0-ARCH.md` | D1-D9 source of record |
| G4-B WAL Retention Decision (APPROVED 2026-08-21) | `governance/decisions/EGBOS-GOV-DECISION-2026-008-S5-35.md` | WAL retention policy |
| S5 Exit Gate | `governance/exit-gates/SLICE-S5-exit-v1.yaml` | Acceptance criteria definitions |
| Market Data Source Specification | `governance/MARKET-DATA-001.yaml` | Provenance, endpoint, and invariant authority |
| S5 Artifact Schema | `governance/schemas/s5_artifact_schema.yaml` | Evidence classification rules |
| S5 Institutional Specification (FROZEN) | `docs/S5_INSTITUTIONAL_SPEC.md` | Frozen S5 requirement registry |
| S5 Tier-0 Provisioning Handoff | `docs/S5_TIER0_PROVISIONING_HANDOFF.md` | Host and environment requirements |
| AC-001 Wi-Fi Decision Package | `governance/decisions/S5_AC001_WIFI_QUALIFICATION_DECISION_PACKAGE.md` | Prior scoped request (superseded by this decision) |
| Governance Gap Decision Package | `governance/decisions/S5_STEP29_GOVERNANCE_GAP_DECISION_PACKAGE.md` | REQ-004 gap authority |
| Board Decision Package (provenance) | `governance/decisions/S5_STEP32_GOVERNANCE_BOARD_DECISION_PACKAGE.md` | Evidence and provenance policy framework |

---

*Prepared by: Governance Decision Assistant*
*Canonical governance directory: governance/decisions/EGBOS-GOV-DECISION-2026-010-S5-NETWORK-MEDIUM.md*
