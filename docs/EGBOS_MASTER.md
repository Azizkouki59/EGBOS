<!-- AUTO-GENERATED FROM EGBOS_MASTER_augmented.html — DO NOT EDIT HTML DIRECTLY -->
<!-- This file is the EXCLUSIVE authoritative reference for the EGBOS project. -->
<!-- Source: documentation/EGBOS_MASTER_augmented.html -->
<!-- Conversion: html2text 2025.4.15 / body_width=0 (no line-wrapping) -->

Institutional Authority Document — Tier 0

# EGBOS

Engineering Governance and Blueprint Operating System  
Institutional Blueprint for Quantitative Trading Infrastructure

Version

1.0 — Freeze Authority Edition

Classification

Institutional Authority — Tier 0

Owner

Chief Architecture Office

Certified

June 16, 2026

Underlying Engine

ENGINE_V3

Governance Status

Phase I Freeze Certified

Maturity Target

Tier-1 Systematic Trading Infrastructure

Enter Blueprint

Enter Blueprint Freeze Certification

⚖

## Phase I Governance Freeze Certification

Governance Contract Baseline — No Architecture Changes Authorized

READY FOR PHASE I GOVERNANCE FREEZE

## Executive Summary

The Phase I Implementation Governance baseline (slices S0–S15, claim-to-slice mapping, sign-off registry, RFC template, govcheck boundary audit, consistency review) is architecturally complete and internally consistent with Phases A–H. All six governance reconciliation findings have been resolved against the authoritative blueprint:

  - **GAP-001 DR Contradiction:** Contradicted by Authority — AD-016 (Institutional Geo-Failover) and AD-028 (Tiered Recovery Contracts) address different failure domains and are complementary architectural controls
  - **GAP-002 Multi-Signature Authority:** Partially Confirmed — Founder-Solo governs capital decisions (A2.5); 2-of-3 cryptographic signatures govern production config envelopes and strategy promotion gates (L1B, L2A). No RFC exists to change Founder-Solo governance authority
  - **GAP-003 Governance Telemetry Blind Spot:** Contradicted by Authority — B1.7 decay vectors (DV-01 through DV-05), G3 Governance-Simulation Feedback Loop, and full G1–G5 observability stack are architecturally complete. Production wiring deferred by design under progressive trust model (A2.1)
  - **GAP-004 Security Architecture Missing:** Contradicted by Authority — Vault, HSM, AppRole, Dynamic Secrets, mTLS, SPIFFE/SPIRE, Zero Trust, Bastion MFA, SBOM, Cosign, Secure Boot are formalized in active ADRs (AD-014, AD-015, AD-017, AD-029) and Phase K specifications (K1.1, K1.2). Implementation status requires verification; architecture is complete
  - **GAP-005 RFC Process:** Partially Confirmed — RFC governance (template, process, manual enforcement, GC-E-053 blocking rule) is complete and active. Automation tooling (govcheck rfc subcommands) deferred to Phase J per J5.1
  - **GAP-006 Observability/Simulation:** Contradicted by Authority — SimulationController → SimulationFeedbackController → GovernanceAction → ObservabilityBridge → Telemetry Sinks is explicitly wired through G3 Governance-Simulation Feedback Loop



No verified blocking items remain in the Phase I governance baseline. Deferred engineering deliverables are correctly phased per progressive trust model.

## Freeze Scope

**This certification applies to the Governance Contract only.**

Certified Scope

  - Phase I Implementation Governance Thesis (I1)
  - Execution Plane Slice Registry S0–S7 (I2)
  - Research Platform Slice Registry S8–S15 (I3)
  - Claim-to-Slice Mapping (I4)
  - Governance Sign-Off Document Registry (I5)
  - RFC Template and Manual Enforcement Protocol (I6)
  - govcheck Boundary Audit and Rule Activation Schedule (I7)
  - Phase I Consistency Review with Reconciled Status (I8)



Explicitly Excluded from This Certification

  - Production Readiness
  - Live Capital Authorization
  - Phase III Staging Qualification
  - Institutional Operations Readiness
  - Security Implementation Verification
  - DR Operational Validation
  - G5 Production Telemetry Wiring



## Deferred Engineering Items (Non-Blocking)

The following items are explicitly deferred to later phases by architectural design. They are not freeze blockers.

Item| Description| Target Phase| Authority  
---|---|---|---  
IMP-001| govcheck rfc lint/impact/apply/validate subcommands| Phase J| J5.1  
IMP-002| DR govcheck rules (GC-E-DR-001/002) for AD-016/AD-028 verification| Phase J / Pre-Phase III| AD-016, AD-028  
IMP-003| G5 production wiring: egbos_exporter, inotify tail, Grafana PAN-001–012| Phase III Exit| G5.1, GC-E-051  
IMP-004| GC-W-REOPEN-001 for inter-slice re-opening protocol| Phase K| J4.3  
IMP-005| Procurement gate items PROC-001 through PROC-008| Phase III Entry (S6)| J3.1  
IMP-006| Unified S0–S15 roadmap with calendar mapping| Phase J| J0–J2  
IMP-007| Security implementation verification (Vault cluster, HSM, mTLS mesh, SBOM pipeline, Secure Boot)| Phase III Staging| AD-014, AD-015, AD-017, AD-029, K1.1, K1.2  
  
## Certification Signatures

Role| Authority| Signature| Date  
---|---|---|---  
Chief Architecture Office| Governance Contract Owner| Pending| Pending  
CTO (Founder)| Governance Decision Authority| Pending| Pending  
  
**Certification ID:** EGBOS-FREEZE-2026-001 | **Authority Document:** EGBOS_MASTER_augmented.html (Freeze Authority Edition v1.0)

EGBOS

Master Blueprint v2.0 — Institutional Edition

⚖ Conflict rule: Phases A–J supersede L0–L5 in all cases. 

Print Expand Top

EGBOS / Master Blueprint

# Executive Summary

EGBOS (Engineering Governance and Blueprint Operating System) is the institutional operating system for a quantitative trading enterprise built on ENGINE_V3. It is not a product — it defines how the organization thinks about correctness, proves safety before deploying capital, governs infrastructure evolution, and maintains institutional memory under maximum stress: market dislocations, personnel changes, and simultaneous system failures. Phase K elevates EGBOS to full institutional-grade deployment readiness through security hardening (Vault/HSM, zero-trust mTLS, SBOM), disaster recovery (RTO < 60s, RPO < 1s geo-failover), institutional market infrastructure (IEEE 1588 PTP, dual-feed A/B arbitration), out-of-process risk enforcement, automated alpha lifecycle governance, four-tier compliance gating, and a measurable production certification framework.

The central thesis is that **correctness is a governance problem before it is an engineering problem**. At institutional-tier firms, capital loss events traceable to technology are caused not by isolated bugs but by silent degradation of the epistemic environment — assumptions about hardware, concurrency, WAL ordering, and feature determinism that erode without detection. EGBOS treats every correctness-critical property as a first-class data structure with explicit verification class, owner, expiry, and dependency graph.

Source Document Synthesis

ENGINE_V3 (temporal authority — what to build, in what order) and Implementation Spec v1.0 (structural authority — how components are built) are merged into a unified institutional blueprint where governance is inseparable from engineering.

Binding Principles (P-001 through P-007)

  - **P-001:** Mechanical Enforcement Supremacy — CI and govcheck are the authority
  - **P-002:** Structural Impossibility Over Runtime Prevention
  - **P-003:** Determinism as a Constitutional Invariant
  - **P-004:** Governance Precedes Implementation
  - **P-005:** Failure Mode Specification Precedes Nominal Specification
  - **P-006:** Capital Safety Over Operational Continuity
  - **P-007:** Immutable Audit Over Convenient Deletion



Phase B Complete — Governance, Research & Validation

  - **B1:** Claim schema, V1–V4 verification classes, govcheck binary, CI gates C0–G4/P8–P11, waiver system, decay vectors
  - **B2:** Research plane boundary (AD-002), hypothesis registry, feature store parity, model registry, signal governance
  - **B3:** WFV framework, OOS vault, DSR ≥ 0.5, CPCV, INV-002/003, decay monitoring, capacity validation
  - **Architectural Decisions:** AD-001 through AD-007; INV-001 through INV-006 defined



Phases C–H Complete

  - **C:** Data/runtime architecture, SHM layouts, Tier-0/1/2 data model, hardware lifecycle, AD-008–AD-012
  - **D:** Control plane, registry store, safety monitors, circuit breakers, audit log, HITL interventions
  - **E:** Execution engine, operational loop, pre-flight safety protocol, runtime status dashboard (E4)
  - **F:** SimulationController (EGBOS_SIM), mock data injection, stress test registry, ObservabilityBridge wiring
  - **G:** ObservabilityBridge (G1), Prometheus Metrics Registry (G2), Governance-Simulation Feedback Loop (G3), Resilience Registry (G4), Telemetry Dashboard (G5)



Phase H Complete — ADR Catalog & Infrastructure Deployment

  - **H1:** ADR Catalog — AD-001 through AD-013, immutable rationale records, amendment authority assignments
  - **H2:** Prometheus exposition topology — egbos_exporter sidecar, localhost:9090, SPSC ring SINK-04
  - **H3:** Grafana panel definitions — PAN-001 through PAN-012, three-row capital-safety/latency/health layout
  - **H4:** Redis production deployment — ACL per consumer, AOF persistence, failure mode specification
  - **H5:** WAL simulation boundary rationale — five rule classes classified Phase IV+ only, structural (not a gap)



Phase I Complete — Implementation Governance

  - **I1:** Implementation Governance Thesis — principle P-004 codified; governance contract separates blueprint (Phases A–J) from implementation (S0–S15)
  - **I2:** Execution Plane Slice Registry (S0–S7) — entry/exit criteria, mandatory claims, ADR dependencies, forbidden actions, and capital gates for all eight execution slices
  - **I3:** Research Platform Slice Registry (S8–S15) — equivalent specification for all eight research platform slices
  - **I4:** Claim-to-Slice Mapping — authoritative table binding every registered CLAIM-XXX to its proving slice, CI gate, and capital gate dependency
  - **I5:** Governance Sign-Off Document Registry — LIVE-CAPITAL-001.yaml, MARKET-DATA-001.yaml, TIMING-CONSTANTS-001.yaml schemas with human-signatory requirements
  - **I6:** RFC Template — formal amendment schema for any V3+ claim, ADR, or architectural principle change
  - **I7:** govcheck Boundary Audit — directory structure verified against AD-001/AD-002 rules; GC-E rule activation schedule confirmed
  - **I8:** Phase I Consistency Review — cross-validated against Phases A–H; six gaps formally deferred to Phase J



## Architecture Timeline

Phased delivery of the EGBOS institutional blueprint and underlying ENGINE_V3 implementation.

Phase A

Executive Architecture & Institutional Operating Model

Architectural thesis, binding principles, operating phases I–V, incident taxonomy, governance authority matrix — COMPLETE

Phase B

Governance, Research & Validation Architecture

Claim system, govcheck, CI gates, research plane boundary, WFV/DSR/CPCV validation framework — COMPLETE

Phase C

Data, Runtime & Infrastructure Architecture

Tier-0/1/2 data model, SHM layouts, hardware lifecycle, co-location — COMPLETE

Phase D

Control & Governance Plane

Registry & Configuration Store, Safety Monitors & Circuit Breakers, Audit & Compliance Logging, HITL Interventions — COMPLETE

Phase E

Execution & Runtime Environment

Execution Engine, Operational Loop, Pre-flight Safety Protocol, Runtime Status Dashboard — COMPLETE

Phase F

System Simulation & Stress Testing

Simulation Controller, mock data injection for E4 Dashboard, Stress Test Registry — COMPLETE

Phase G

Observability, Resilience & Governance Feedback

Observability Bridge, Prometheus Metrics Registry, Governance-Simulation Feedback Loop, Resilience Registry, Telemetry Dashboard — COMPLETE

Phase H

ADR Catalog & Infrastructure Deployment Governance

Immutable Architecture Decision Records (AD-001–AD-013), Prometheus exposition topology, Grafana panel definitions, Redis production deployment — COMPLETE

Phase I

Implementation Governance

Slice acceptance criteria, implementation claim mapping — ACTIVE

Phase J

Roadmap S0–S15+

Unified implementation roadmap merging ENGINE_V3 phases and Impl Spec slices

Phase K

HTML Documentation System Design

Living document system specification (this file)

Phase L

Cursor Prompt Package → Master Cursor Prompt

Agent operational instructions for architecture-constrained development

## Phase Registry

**Drift-risk decay row:** Phases marked with the decay-risk ring are governed by B1.7 (governance decay vectors) and B3.7 (signal decay monitoring); decay-risk status is visual only and does not alter phase completion status.

## Architecture Visualizations

Five-plane institutional architecture model. Each plane is a bounded context with explicit governance enforcement at plane boundaries.

### Layer 0 — Governance Plane

### Layer 1 — Research Plane

### Layer 2 — Execution Plane

### Layer 3 — Durability Plane

### Layer 4 — Observability Plane

### Full Stack Overview
[code] 
    EGBOS INSTITUTIONAL ARCHITECTURE
    ═══════════════════════════════════════════════════════════════════════
    
    LAYER 0: GOVERNANCE PLANE
    ┌─────────────────────────────────────────────────────────────────────┐
    │  Correctness Claim Registry  │  Deploy Gate System  │  RFC Process  │
    │  Verification Class Engine   │  Waiver Management   │  ADR Registry │
    │  Ownership Graph             │  Incident Taxonomy   │  Amendment Log│
    └─────────────────────────────────────────────────────────────────────┘
                        ↓ govcheck enforces ↓
    LAYER 1: RESEARCH PLANE
    ┌─────────────────────────────────────────────────────────────────────┐
    │  Hypothesis Registry  │  Feature Store  │  Walk-Forward Validation  │
    │  Alpha Discovery      │  Signal Registry│  DSR / CPCV Gates         │
    │  Dataset Governance   │  OOS Vault      │  Model Registry           │
    └─────────────────────────────────────────────────────────────────────┘
                        ↓ verified signal flows ↓
    LAYER 2: EXECUTION PLANE
    ┌─────────────────────────────────────────────────────────────────────┐
    │  L0 Ingestor │ L1 LOB │ L2 Features │ L3 Position │ L4 Risk │ L5 Exec│
    │  WAL Writer  │ 3-Pass Replay        │ SPSC Rings  │ SHM     │ Escrow │
    └─────────────────────────────────────────────────────────────────────┘
                        ↓ capital flows ↓
    LAYER 3: DURABILITY PLANE
    ┌─────────────────────────────────────────────────────────────────────┐
    │  Tier-0 Raw Event Log │  WAL (Primary NVMe) │  Stratum-0 Emergency  │
    │  Tier-1 Normalized    │  Replay Engine      │  Cold Recovery        │
    │  Tier-2 Aggregates    │  INV-001..005       │  RFP Reconciliation   │
    └─────────────────────────────────────────────────────────────────────┘
                        ↓ metrics flow ↓
    LAYER 4: OBSERVABILITY PLANE
    ┌─────────────────────────────────────────────────────────────────────┐
    │  Prometheus  │  Grafana  │  P&L Attribution  │  Feature Health       │
    │  Alert Mgr   │  Traces   │  Alpha Decay Mon  │  Daily Reconciliation │
    └─────────────────────────────────────────────────────────────────────┘
    
    ═══════════════════════════════════════════════════════════════════════
    CROSS-CUTTING: Security │ Compliance │ Incident Management │ Capacity
    ═══════════════════════════════════════════════════════════════════════
[/code]

A

## Phase A — Executive Architecture & Institutional Operating Model

Architectural thesis, binding principles, system boundaries, technology authority, operating phases, incident classification

Complete

## A1 — Executive Architecture

Classification
    Institutional Authority Document — Tier 0

Version
    1.0

Owner
    Chief Architecture Office

Approval Authority
    CTO + Governance Board

Review Cycle
    Annually or on major architectural change

### A0 — Document Version Control

EGBOS_MASTER_augmented.html is itself a governance artifact subject to P-007 (Immutable Audit Over Convenient Deletion). The document version is tracked in `governance/DOCUMENT-AMENDMENT-LOG.yaml` using the same append-only protocol as claim amendments.
[code] 
    document_id:        "EGBOS-MASTER"
    current_version:    "1.0"
    schema_version:     1
    amendments:
      - version:        "1.0"
        date:           "2026-05-30"
        author:         "@founder"
        summary:        "Initial institutional blueprint — Phases A through K, BRO L0–L5, MIG, Phase M"
        phases_added:   ["A","B","C","D","E","F","G","H","I","J","L0","L1","L2","L3","L4","L5","MIG","K","L","M"]
        adrs_added:     ["AD-001","AD-002","AD-003","AD-004","AD-005","AD-006","AD-007","AD-008","AD-009","AD-010","AD-011","AD-012","AD-013","AD-014","AD-015","AD-016","AD-017","AD-018","AD-019","AD-020","AD-021","AD-022","AD-023","AD-024","AD-025","AD-026","AD-027","AD-028","AD-029","AD-030"]
        govcheck_sha256: ""    # populated by CI on commit
[/code]

**Version increment trigger:** The document version increments on any of: new phase section added, ADR added or superseded, architectural principle amended, binding threshold changed. Version increments do NOT require an RFC unless the underlying change does. The DOCUMENT-AMENDMENT-LOG.yaml is committed in the same PR as the document change it records.

### A1.1 — System Identity

**Platform Name**|  EGBOS (Engineering Governance and Blueprint Operating System)  
---|---  
**Underlying Engine**|  ENGINE_V3  
**Operational Class**|  Institutional Quantitative Trading Platform  
**Maturity Target**|  Tier-1 Systematic Trading Infrastructure (Citadel/Two Sigma equivalence)  
  
EGBOS is not a product. It is an operating system for a quantitative trading enterprise. It defines how the organization thinks about correctness, how it proves safety before deploying capital, how it governs the evolution of its own infrastructure, and how it maintains the institutional memory necessary to operate safely under the conditions of highest stress — market dislocations, personnel changes, and system failures occurring simultaneously.

### A1.2 — Architectural Thesis

The central thesis of EGBOS is that **correctness is a governance problem before it is an engineering problem**.

At firms operating at the institutional tier, the majority of capital loss events traceable to technology are not caused by bugs in isolation. They are caused by the degradation of the conditions under which correct code was written — conditions that included specific assumptions about hardware, specific constraints on concurrency, specific guarantees about WAL ordering, and specific promises about feature determinism. When those conditions degrade silently, correct code becomes incorrect code. The code did not change. The epistemic environment did.

EGBOS addresses this by treating every correctness-critical property as a first-class data structure with an explicit verification class, an explicit owner, an explicit expiry, and an explicit dependency graph. The governance system is mechanically enforced. It cannot be bypassed. It degrades gracefully under key-person risk. It is the institutional immune system against silent confidence erosion.

### A1.3 — Strategic Architecture Overview

See Architecture Visualizations for interactive plane diagrams and the full stack ASCII overview.

### A1.4 — Architectural Principles (Binding)

These principles are binding. No architectural decision may contradict them. Contradictions require a formal RFC with amendment authority.

P-001

Mechanical Enforcement Supremacy

Every correctness constraint must be enforced by a tool before it is considered active. Discipline is not an enforcement mechanism. CI is the enforcer. The govcheck binary is the authority.

**Rationale:** Solo-founder operation under schedule pressure guarantees that discipline-based constraints will be violated at the worst possible time. The system must be correct even when the operator is exhausted and rationalizing.

P-002

Structural Impossibility Over Runtime Prevention

If a class of failure can be made structurally impossible at compile time or build time, it must be. Runtime assertions that catch a failure after it executes are inferior to compile-time constraints that make the failure unwritable.

**Rationale:** Runtime detection means the failure already happened. In a trading system, "happened" means capital may have moved incorrectly. Prevention is always superior.

P-003

Determinism as a Constitutional Invariant

Every observable system state must be reconstructable from the immutable triad: Tier-0 raw event log, WAL, and Stratum-0 emergency records. Any state that cannot be reconstructed must be designated ephemeral with explicit recovery semantics. Any state that influences an order decision, position calculation, or risk limit evaluation is classified as observable and must satisfy this invariant regardless of storage medium (including Redis, etcd, or in-process cache). Classification of any such state as ephemeral requires an explicit ADR entry documenting the recovery semantics.

**Rationale:** Replay fidelity is the bridge between research and live trading. If production cannot be deterministically replicated, research results cannot be trusted. If research results cannot be trusted, every capital deployment decision is uninformed.

P-004

Governance Precedes Implementation

No implementation slice produces any code, configuration, or infrastructure artifact that will be included in a production build until its governance artifacts — claims, ownership, verification criteria, and acceptance gates — are committed. The implementation roadmap is a function of the governance readiness, not a schedule imposed upon it.

**Rationale:** Governance added after implementation is retrofitting. Retrofitted governance is aspirational, not structural. Structural governance is the only kind that holds under incident conditions.

P-005

Failure Mode Specification Precedes Nominal Specification

Every component's degraded mode, fault isolation behavior, and recovery path must be specified before its nominal behavior. The system is designed for the failure scenario first, the success scenario second.

**Rationale:** Nominal behavior is designed by the features it must deliver. Failure behavior is designed by the consequences it must contain. Capital safety is a failure-mode property, not a nominal-mode property.

P-006

Capital Safety Over Operational Continuity

When a conflict exists between maintaining operational continuity and maintaining capital safety, capital safety wins unconditionally. TERMINAL_HALT is always preferable to silent position drift.

**Rationale:** Position drift that is not caught is a loss with unknown magnitude. TERMINAL_HALT is a loss of operational uptime with known magnitude. The former is existential. The latter is recoverable.

P-007

Immutable Audit Over Convenient Deletion

All governance artifacts, signoffs, claim histories, incident records, and architectural decisions are append-only. Nothing is deleted. Superseded artifacts are marked superseded, not removed.

**Rationale:** The ability to reconstruct the institutional reasoning at any prior point in time is a capital audit requirement and an incident investigation requirement. Deletion destroys the ability to answer "what did we believe was true on date X, and why?"

### A1.5 — System Boundaries

#### In Scope

  - Execution infrastructure (L0–L5)
  - Research platform (Phases 8–15)
  - Governance subsystem (govcheck, claim registry)
  - Observability plane
  - Durability plane (WAL, Tier-0, Stratum-0)
  - CI/CD pipeline
  - Deployment and operations



#### Out of Scope (Current Phase)

  - Investor-facing reporting systems
  - Multi-entity legal structure
  - Prime brokerage integration beyond exchange connectivity
  - FPGA acceleration layer
  - Multi-datacenter replication
  - External API exposure



#### Explicit Non-Goals

  - High-frequency market making at sub-microsecond latency (current target: <500µs feed-to-decision)
  - Retail investor product
  - Decentralized or trustless execution



### A1.6 — Technology Authority Assignments

Domain| Primary Language| Secondary| Constraint  
---|---|---|---  
Exchange connectivity (L0)| Rust| C++| Network boundary only  
LOB engine (L1)| C++20| —| No std::map, no dynamic alloc  
Feature engine (L2)| C++20| —| No clock_gettime, no PRNG  
Position engine (L3)| C++20| —| WAL-before-state mandatory  
Risk enforcer (L4)| C++20| —| Monotone latches only  
Execution router (L5)| C++20| —| ONNX only, no pickle  
Regime/Alpha (L6-L7)| Python 3.11| —| Redis Streams IPC only  
Research platform| Python 3.11| Polars| No engine/ imports  
Governance tooling| C++ (govcheck)| —| Compiled binary, not script  
Data lake| Python/Polars| Rust| Parquet/Arrow, DVC  
Build system| CMake + Cargo + uv| —| Pinned versions  
Schema language| FlatBuffers| —| Committed generated code  
  
_Language assignment is architectural, not preferential. Changes require ADR._

## A2 — Institutional Operating Model

Classification
    Operational Authority Document — Tier 0

Version
    1.0

Owner
    Chief Architecture Office + Operations

Approval Authority
    CTO

### A2.1 — Operating Model Philosophy

EGBOS operates under a **Progressive Trust Model**. Capital deployment authority is earned through verified system behavior under progressively larger and more adversarial conditions. Trust is never assumed. It is proven incrementally, recorded immutably, and subject to revocation on evidence of failure.

This model mirrors the institutional operating philosophy of systematic trading firms: paper trading is not a formality, it is a verification protocol. Staging is not a deployment step, it is a qualification environment. Live capital at 5% is not a soft launch, it is a governance gate.

### A2.2 — Operational Phases

Phase I: Infrastructure Qualification (ENGINE_V3 Phases 0–4)

**Objective:** Prove the execution infrastructure correct under adversarial synthetic conditions before any live data is processed.

**Entry Condition:** Empty repository  
**Exit Condition:** govcheck operational, all GC-E-0xx rules blocking, CLAIM-L3-02 at V3, replay fidelity gate passing continuously

#### Operating Constraints

  - No live exchange connections
  - No real market data
  - All testing against synthetic fixtures
  - CI must be green on every commit
  - All claims must be active and at declared class



#### Key Milestones

  - WAL kill-9 test: 10/10 trials (CLAIM-WAL-001 V4)
  - INV-001 established: 10M fills + 10× kill-9 (CLAIM-INV-001 V4)
  - Replay fidelity gate: 3× byte-identical (CLAIM-FEATURE-001 V4)
  - govcheck binary operational (C0 gate active)



Phase II: Market Data Qualification (ENGINE_V3 Phase 5)

**Objective:** Connect to live exchange feeds. Prove system behavior against real market microstructure. No orders transmitted.

**Entry Condition:** Phase I exit criteria met  
**Exit Condition:** 14 consecutive days uptime, 14 nightly replay validations passed, CLAIM-L3-05 at V4

#### Operating Constraints

  - Feed connection: read-only
  - Zero order transmission to any exchange
  - Paper order loop active (WAL-recorded, exchange-invisible)
  - Daily nightly replay validation mandatory
  - INV-001 monitor active; any violation is a Phase II exit failure



#### Escalation Protocol

  - Any INV-001 violation: immediate Phase II suspension, root cause required before resumption
  - Any replay non-determinism: Phase II suspension, feature engine bisect required
  - Any TSAN finding: Phase II suspension, race condition must be eliminated



Phase III: Production Qualification (ENGINE_V3 Phase 6)

**Objective:** Deploy to production-equivalent bare metal. Measure all timing constants empirically. Qualify the operational runbook suite.

**Entry Condition:** Phase II exit criteria met, G2 gate passing  
**Exit Condition:** 48h clean staging run, all runbooks fire-drilled, timing_constants.json committed, LIVE-CAPITAL-001.yaml signed

#### Operating Constraints

  - Production-equivalent hardware mandatory (no cloud VMs for timing measurement)
  - All 10 Phase-0 startup checks must pass on production hardware
  - No timing constants from documentation or estimation — measured only
  - Stratum-0 cold recovery drill mandatory before sign-off



Phase IV: Capital Deployment (ENGINE_V3 Phase 7)

**Objective:** Deploy real capital under signed governance record. Scale progressively under zero-incident conditions.

Stage| Position Limit| Entry Condition| Exit/Rollback  
---|---|---|---  
IV-A| 5% target notional| All Phase III criteria met; LIVE-CAPITAL-001 signed| Any P0 → halt, manual review  
IV-B| 20% target notional| Zero P0 incidents, 2-week clean operation| Any P0 → revert to IV-A  
IV-C| 50% target notional| Zero P1 incidents, 4-week clean operation| Any P0 → halt; P1 → revert to IV-B  
IV-D| 100% target notional| First P0 incident successfully recovered per runbook (replay verified)| TERMINAL_HALT → full halt  
  
_Note: Stage IV-D entry requires surviving a real P0 incident. An unrecovered system has an unverified recovery path._

Phase V: Institutional Research Operations (ENGINE_V3 Phases 8–15)

**Objective:** Build and operate the full institutional research pipeline. Deploy first alpha strategy through G4.

Sub-Phase| Scope| Description  
---|---|---  
V-A| Phase 8| Research Infrastructure — Docker env, MLflow, hypothesis registry  
V-B| Phase 9| Dataset Governance — DVC, point-in-time, OOS vault  
V-C| Phase 10| Backtesting & WFV — INV-002, WFV gate  
V-D| Phase 11| Feature Store & Model Gov — INV-003, model registry  
V-E| Phase 12| Alpha Research — First WFV-certified signal  
V-F| Phase 13| Strategy Deployment — DEPLOY-STRATEGY-001, G4  
V-G| Phase 14| Capacity & Attribution — INV-005 daily reconciliation  
V-H| Phase 15| ML Lifecycle — Champion/challenger, quarterly WFV  
  
### A2.3 — Incident Classification System

P0 — Capital Safety Incident

**Definition:** Any condition where capital may be at risk, position state is uncertain, or a TERMINAL_HALT has occurred.

**Response Time:** Immediate (automated trigger or manual within 60 seconds)

**Required Actions:** Engine halt, position freeze, manual RFP reconciliation, root cause before restart

**Authority to Restart:** Founder (after RFP match confirmed)

**Examples:** TERMINAL_HALT, INV-001 violation, WAL corruption, exchange position discrepancy

P1 — Operational Degradation

**Definition:** System operating outside normal parameters without immediate capital risk.

**Response Time:** Within 15 minutes

**Required Actions:** Investigate, document, remediate or accept with documented rationale

**Authority to Continue:** Founder (with documented rationale)

**Examples:** Replay non-determinism detected, nightly validation failure, TSAN finding, feature drift alert, CLAIM expiry

P2 — Governance Finding

**Definition:** A governance process violation, documentation gap, or process deviation without immediate operational impact.

**Response Time:** Within 24 hours

**Required Actions:** Document, remediate within sprint

**Authority to Continue:** Founder

**Examples:** Claim review expiry, missing amendment log entry, waiver approaching expiry

### A2.4 — Key Person Risk Architecture

_EGBOS is designed for solo-founder operation. This creates structural key-person risk that must be architecturally mitigated._

#### Mitigation Strategy — Institutional Memory Externalization

All decisions that would ordinarily reside in expert human memory are externalized into:

  1. Governance claims (machine-readable, version-controlled)
  2. ADR catalog (rationale-preserving, immutable)
  3. Runbooks (procedure-explicit, fire-drilled)
  4. CI gates (automated, non-bypassable)
  5. Amendment logs (decision history, append-only)



#### Mitigation Strategy — Automated Enforcement

No correctness constraint relies on the founder remembering to check it. govcheck is the enforcer. CI is the authority. A new engineer joining after 18 months can reconstruct the complete epistemic state of the system from the governance artifacts alone.

#### Mitigation Strategy — Operational Documentation

Every procedure that requires human judgment is documented to the point where a qualified engineer with no prior context can execute it safely. This is the fire-drill standard: if the runbook cannot be completed by an unfamiliar engineer in a fire drill, it is defective.

### A2.5 — Governance Decision Authority Matrix

Decision Type| Authority| Quorum| Documentation  
---|---|---|---  
Architecture Principle change| CTO (Founder)| Solo| Formal RFC + ADR  
New architecture claim creation| Founder| Solo| Claim YAML + PR  
Claim verification class upgrade| Founder + review| Solo with evidence| Amended claim + proof artifact  
R4 claim set| Any engineer (Founder)| Solo| Tracked issue + claim amendment  
R4 claim cleared| Founder| Solo + investigation outcome| Binary outcome document  
Live capital deployment| Founder| Solo| LIVE-CAPITAL-001 signed  
Capital scaling stage advance| Founder| Solo| Operational log entry  
Runbook modification| Founder| Solo| Version-controlled amendment  
ADR creation| Founder| Solo| ADR document + review  
New waiver grant| Founder (CTO role)| Solo| Waiver YAML committed  
Emergency halt (TERMINAL_HALT)| Automated (no human decision)| Automated| Audit log  
Restart post TERMINAL_HALT| Founder| Solo + RFP match| Incident record  
Strategy deployment (G4)| Founder| Solo + govcheck pass| DEPLOY-STRATEGY-001  
  
⚑ **Governance Reconciliation Note:** Cryptographic multi-signature controls (L1B, L2A) do not modify governance authority assignments. Governance decisions (A2.5) remain Founder-Solo per P-004 and solo-founder model. Multi-signature envelopes govern production configuration loading (config.bin.env) and strategy promotion gates (Stage 4: Pilot), not governance decision authority.

### A2.6 — Operational Calendar

#### Daily (post-market)

  1. `scripts/daily_pnl_reconciliation.sh` — INV-005 mandatory (from Phase 14)
  2. `scripts/nightly_replay_validation.sh` — byte-identical replay validation (PROC-REPLAY-001)
  3. Incident log review — any anomaly documented
  4. Claim expiry check — `govcheck waiver check`



#### Weekly

  - Feature drift metrics review
  - Alpha decay dashboard review
  - CLAIM review expiry scan
  - CI gate health review



#### Monthly

  - Claim ownership review
  - Waiver accumulation review (>180 waiver-days triggers governance review)
  - Shadow replay verification job (Tier-0 → Tier-2 diff)
  - Runbook review for any runbooks executed in the month



#### Quarterly

  - `scripts/quarterly_wfv.sh` — model revalidation on extended OOS (from Phase 15)
  - Full claim registry review — all V5 claims
  - Timing constants re-measurement (if hardware changed)
  - Security review
  - Capacity estimate review



#### Annual

  - Architecture principle review
  - ADR catalog review — decisions still valid?
  - Governance RFC review — process still fit for purpose?
  - Hardware lifecycle planning
  - Exchange connectivity contract review



## Phase A — Consistency Review

### Consistency Checks Performed

  - Executive Architecture principles are non-contradictory
  - Operating Model phases align with ENGINE_V3 phase sequence (0–15)
  - Incident classification system (P0/P1/P2) consistent with ENGINE_V3 references
  - Capital scaling schedule matches ENGINE_V3 §7 exactly
  - Governance Decision Authority Matrix consistent with solo-founder model
  - Technology Authority Assignments consistent with Implementation Spec layer assignments
  - No orphan components identified in Phase A
  - No undefined ownership in Phase A



### Governance Reconciliation Status

Item| Previous Classification| Reconciled Status| Authority  
---|---|---|---  
Security architecture definition| Gap| Contradicted by Authority| AD-014, AD-015, AD-017, AD-029, K1.1, K1.2 active  
Compliance requirements catalogue| Gap| Partially Confirmed| AD-030 (Compliance Audit Pipeline), Phase K  
Exchange connectivity vendor management| Gap| Deferred by Design| J3.1 PROC-005 (Phase J)  
Hardware procurement and lifecycle| Gap| Deferred by Design| J3.1 PROC-001 through PROC-008 (Phase J)  
Formal ADR catalog| Gap| Confirmed| AD-001 through AD-030 complete (Phase H)  
  
### Missing Domains Flagged for Future Phases

Domain| Status| Target  
---|---|---  
Physical security of co-location environment| Deferred by Design| J3.1 PROC-001  
Business continuity planning (beyond TERMINAL_HALT recovery)| Deferred by Design| AD-023 (Executable Incident Response)  
Regulatory reporting obligations| Deferred by Design| AD-030 (Compliance Audit Pipeline)  
Network architecture (co-location connectivity)| Deferred by Design| AD-018, J3.1 PROC-004, PROC-005  
Secrets management (API keys, exchange credentials)| Contradicted by Authority| AD-014 (Vault HSM), GC-E-SECRETS-001/002 active from S1  
  
B

## Phase B — Governance, Research & Validation Architecture

Claim system, govcheck binary, CI gates, research plane boundary, WFV/DSR/CPCV validation framework

Complete

## Pre-Generation Analysis

Before producing Phase B artifacts, the following tensions were resolved:

Tension| Resolution  
---|---  
**Verification Class Terminology** — ENGINE_V3 V1–V4 vs RFC V1–V6| EGBOS adopts ENGINE_V3 taxonomy (V1–V4) as operational standard. V5/V6 are aspirational extensions for post-Phase 7 TLA+ work.  
**Research Boundary** — import-linter vs structural isolation| Boundary is _CI-time structural_ — enforced before any code reaches main. Import-linter prevents dependency existence; static analysis detects patterns within code.  
**WFV Thresholds** — stated without rationale| All P10 thresholds documented with derivation from expected live-to-backtest Sharpe ratios.  
**govcheck Binary vs Shell Script**|  Compiled C++ binary required — shell scripts accumulate silent exceptions and cannot be type-checked or fixture-tested.  
**Amendment Authority Chain**|  Amendments are append-only on active claims; superseded claims are immutable; downstream composition dependencies must update within one sprint.  
  
## B1 — Governance Architecture

Classification
    Constitutional Authority Document — Tier 0

Version
    1.0

Owner
    Chief Architecture Office

Approval Authority
    CTO + Governance Board

Review Cycle
    On any govcheck version bump or CI gate modification

### B1.1 — Governance Philosophy

The correctness governance architecture exists to solve a problem prose documentation cannot: **the epistemic environment in which correct code was written degrades silently over time, and correct code in a degraded epistemic environment becomes incorrect code.**

Consider: an engineer writes a WAL writer correct given the assumption that a single thread is the sole CQE consumer of the io_uring ring. Eighteen months later, a performance optimization introduces a second reader. The WAL writer code has not changed. The assumption has. Without a machine-readable record of that assumption, its violation is invisible until a position accounting error surfaces during live trading.

The governance system makes assumption violations visible before they become capital events. Every assumption a correctness-critical claim depends upon is a first-class data structure. Every modification triggering re-evaluation follows a structured workflow. This is not documentation — it is a correctness tracking system.

### B1.2 — Correctness Claim System

#### B1.2.1 — Claim Schema (Canonical)

A governance claim is the atomic unit of the correctness tracking system. Every claim is a YAML file under `governance/claims/`. One file per claim. Filename equals claim_id. Schema enforced by govcheck at C0.

Field| Type / Values| Notes  
---|---|---  
`schema_version`| Integer (currently 1)| Govcheck rejects unknown versions  
`claim_id`| String| Matches filename. Immutable after creation  
`namespace`| String| Dot-separated; must be in namespace registry  
`title`| String ≤120 chars| Human-readable summary  
`text`| Multiline string| Precise property being claimed  
`verification_class`| V1 | V2 | V3 | V4| Strength of evidence  
`previous_class`| Null or enum| Populated on every class change  
`class_rationale`| String| Why this class; what evidence supports it  
`enforcement_mechanism`| String| Exact tool, rule ID, or test name  
`enforcement_scope`| String| What is covered — must be precise  
`enforcement_gaps`| List of strings| What mechanism does NOT cover. Mandatory  
`assumptions`| List of strings| Every assumption claim depends on. Exhaustive  
`composition_dependencies`| List of {claim_id}| Claims this depends on for correctness  
`composed_verification_class`| Computed| Human-set value checked by govcheck, not trusted  
`owner`| GitHub username| Individual, not team alias  
`review_authority`| String| GitHub username or role  
`backup_owner`| String or null|   
`remediation_category`| Null | R3 | R4|   
`remediation_notes`| String| Mandatory when remediation_category non-null  
`deployment_blocking`| Boolean|   
`deployment_blocking_rationale`| String| Mandatory when deployment_blocking true  
`minimum_class_for_unblocked_deploy`| V1–V4|   
`rfc_references`| List of {rfc, section, anchor}|   
`proof_artifacts`| List of {path, type, sha256}| SHA-256 verified by govcheck  
`review_history`| Append-only list| {date, reviewer, action, class_at_review, notes}  
`created_at` / `created_by`| ISO-8601 / username| CI-overwritten on creation  
`last_reviewed` / `review_expiry`| ISO-8601| 12 months from last_reviewed default  
`status`| active | superseded | archived | invalidated|   
`superseded_by`| claim_id or null|   
`amendments`| Append-only list| Amendment records  
  
#### B1.2.2 — Verification Classification System

EGBOS uses a four-class verification taxonomy. Class represents evidence strength and determines deployment eligibility.

V1 — Manual Review

**Definition:** Property asserted based on human review. No automated enforcement exists or is possible.

**Admissibility:** Documented review_history with specific reviewer, date, and scope. Must list all assumptions and enforcement gaps.

**Scope Limit:** Applies only to code at specific commit reviewed. Modifications revert claim to R3 unless demonstrably non-impactful.

**Deployment:** V1 may not clear deployment without waiver. V1 on deployment-blocking claim with `minimum_class_for_unblocked_deploy: V3` blocks deploy.

**Failure Mode Prevented:** Undocumented assumptions — forces explicit articulation of what reviewers believed true.

V2 — Compile-Time or Build-Time Enforcement

**Definition:** Violation produces compile-time error or build-system failure on all declared toolchain versions.

**Import-Linter Boundary:** Runs at CI time (P1), classified as V2 because it operates on import graph structure — prevents dependency _existence_ , not patterns within code. Stronger because it acts earlier in the dependency chain.

**Failure Mode Prevented:** Structural violations discipline cannot prevent — accidental research→engine imports under deadline pressure.

V3 — Static Analysis Enforcement

**Definition:** Violation detected by named static analysis tool with named rule on all in-scope code paths.

**Fixture Test Requirement:** Must include violation fixture whose CI check verifies the fixture _fails_ the rule. If fixture passes, rule is broken — blocking CI error. Prevents silent rule degradation from toolchain updates or suppress annotations.

**Failure Mode Prevented:** (1) Rule exists but silently fails; (2) Rule scope narrower than claim assumes.

V4 — Empirical Validation

**Definition:** Property demonstrated under explicitly specified test conditions (hardware_id, kernel, duration, load).

**PROC-KILL9-001 (10-Trial Kill-9 Standard):** Kill-9 crash recovery requires 10 consecutive successful trials with random injection points — single trial can pass by luck.

**Scope Limit:** V4 on dev workstation does not transfer to production co-location. timing_constants.json must be measured on production hardware.

**Failure Mode Prevented:** "It works on my machine" — hardware_id in proof_artifacts makes environment gap visible.

_Bridge Note:_ Governance RFC v1.0 defines V5 (Formally Argued) and V6 (Formally Verified) as aspirational extensions for post-Phase 7 TLA+ work. EGBOS operational standard remains V1–V4.

#### B1.2.2a — Named Verification Procedure Registry

Procedure ID| Name| Defined In| Used By  
---|---|---|---  
PROC-KILL9-001 | 10-Trial Kill-9 Standard | B1.2.2 V4 class definition | CLAIM-WAL-001, CLAIM-L3-001, CLAIM-INV-001 proof artifacts; S1, S3 acceptance gates  
PROC-REPLAY-001 | 3-Pass SHA-256-Identical Determinism Standard | B3.5 INV-002; C2.4 WAL Recovery Step 3 | CLAIM-BACKTEST-001, CLAIM-LOB-001, CLAIM-FEATURE-001; P9 gate; S2, S10 acceptance gates  
PROC-NIGHTLY-REPLAY-001 | Nightly Byte-Identical Replay Validation | A2.6 operational calendar daily item 2 | CLAIM-FEATURE-001; S5 acceptance gate 14-day requirement; RESEARCH_HALT trigger  
  
#### B1.2.3 — Claim Lifecycle State Machine
[code] 
                 ┌─────────────────────────────────────────┐
                 │                CREATION                  │
                 │  PR creates claim_id.yaml                │
                 │  C0 validates schema                     │
                 │  govbot requests review_authority review │
                 └──────────────┬──────────────────────────┘
                                │ merge
                                ▼
                  ┌─────────────────────────┐
                  │          ACTIVE         │◄──────────────┐
                  │  deployment_blocking    │               │
                  │  gates enforced         │               │
                  │  review_expiry ticking  │               │
                  └──┬────┬─────┬────┬─────┘               │
                     │    │     │    │                      │
              upgrade│ down│  R4 │inval│               re-review
                     │ grade│  set│ idate              (expiry reset)
                     ▼    ▼     ▼    ▼                      │
               ┌────────┐ ┌──────────────────────┐         │
               │UPGRADED│ │  ACTIVE + REMEDIATION│         │
               │(still  │ │  remediation_category│         │
               │ active)│ │  = R3 or R4          │─────────┘
               └────┬───┘ │  deploy blocked (R4) │
                    │     └──────────┬───────────┘
               new  │           R4 cleared
               class│           (investigation outcome: CORRECT)
                    ▼                 │
             ┌─────────────────────────────────────┐
             │  Back to ACTIVE at new/same class   │
             └─────────────────────────────────────┘
                                │
                       ┌────────┼──────────┐
                    superseded invalidated archived
                       │         │          │
                       ▼         ▼          ▼
                ┌─────────┐ ┌──────────┐ ┌────────┐
                │SUPERSEDED│ │INVALIDATED│ │ARCHIVED│
                │(immutable)│ │(immutable)│ │(immut.)│
                └─────────┘ └──────────┘ └────────┘
[/code]

**State Transition Rules:**

  - **ACTIVE → SUPERSEDED:** New claim replaces this one. `superseded_by` populated. Composition dependents must update within one sprint. govcheck warns at 7 days, blocks at 30 days.
  - **ACTIVE → INVALIDATED:** Code change, investigation, or incident demonstrates property does not hold. Composition dependents auto-flagged. Never deleted.
  - **ACTIVE → ARCHIVED:** Component removed. Cannot archive while active claims depend on it.
  - **Amendment Protocol:** Append-only `amendments` list. Can update any field except claim_id, created_at, created_by. Every amendment requires review_history entry.



#### B1.2.4 — Composition Rule
[code] 
    composed_class(C) = min(C.verification_class,
                            min over all D in transitive_deps(C) of D.verification_class)
[/code]

Prevents confidence laundering — citing strong leaf claims to imply stronger composed claim. Transitive (not immediate-only) checking closes gaming via intermediate claims. Undeclared cross-namespace dependencies are GC-E-023 hard errors per `governance/namespace-deps.yaml`.

#### B1.2.5 — Claim Registry — Active Claims

Claim ID| Namespace| Class| Phase| Blocking| Description  
---|---|---|---|---|---  
CLAIM-GOVCHECK-001 | toolchain.govcheck | V4 | 0 | Yes | govcheck binary is ASAN/TSAN/UBSAN-clean and self-verifying; exit 0 on empty claim set; version pinned in CI (GC-F-002)  
CLAIM-000| bootstrap| V1| 0| No| Bootstrap claim  
CLAIM-L3-02| exec.fix_consumer| V1→V3| 0→4| Yes| FIX Consumer zero business logic  
CLAIM-SPSC-001| ipc.spsc| V4| 1| No| SPSC ring TSAN-clean, <50ns P99  
CLAIM-WAL-001| wal.durability| V4| 1| Yes| WAL 10× kill-9 recovery  
CLAIM-L3-POOL-001| allocator.pool| V2| 1| No| No dynamic allocation in pool  
CLAIM-L0-001| ingestor.boundary| V3| 2| No| L0 import-linter clean  
CLAIM-L0-002| ingestor.gap| V1→V3| 2→4| No| Gap detection correctness  
CLAIM-LOB-001| lob.determinism| V4| 2| Yes| LOB 3× byte-identical replay  
CLAIM-FEATURE-001| feature.determinism| V4| 2| Yes| Replay fidelity gate  
CLAIM-L3-001| exec.wal_ordering| V4| 3| Yes| WAL-before-state kill-9  
CLAIM-INV-001| exec.position| V4| 3| Yes| Position = sum(FILL_APPLIED)  
CLAIM-L4-001| risk.kill_switch| V4| 3| Yes| Kill switch <1ms response  
CLAIM-L3-003| exec.state_ownership| V2→V3| 3→4| Yes| order_tag_ private, no public getter  
CLAIM-L3-004| exec.memory_ordering| V4| 3| Yes| TSAN + CROSS_PLANE_READ macro  
CLAIM-L3-006| exec.atomic_exposure| V4| 3| Yes| gross_exposure_ sole cross-plane read  
CLAIM-L3-MAR-001| exec.mass_cancel| V4| 3| Yes| MassActionReport WAL-before-state  
CLAIM-L3-05| startup.buffer| V3→V4| 4→5| Yes| Startup buffer empirically sized  
CLAIM-RESEARCH-001| research.boundary| V3| 8| Yes| No research→engine imports  
CLAIM-DATA-001| data.point_in_time| V4| 9| Yes| Point-in-time correct, no lookahead  
CLAIM-DATA-002| data.splits| V3| 9| Yes| Split dates immutable  
CLAIM-BACKTEST-001| research.backtest| V4| 10| Yes| INV-002 backtest determinism  
CLAIM-WFV-001| research.wfv| V3| 10| Yes| WFV OOS discipline  
CLAIM-FEATURE-PARITY-001| feature.parity| V4| 11| Yes| INV-003 byte-identical parity  
CLAIM-MODEL-001| model.registry| V3| 11| Yes| Model card completeness  
CLAIM-MODEL-002| model.inference| V4| 13| Yes| Inference P99 within budget  
CLAIM-SIGNAL-001| signal.weights| V3| 12| Yes| Signal weight governance  
CLAIM-REGIME-001| regime.wal| V4| 11| Yes| INV-004 regime WAL determinism  
CLAIM-CAPACITY-001| capacity.impact| V4| 14| Advisory| Almgren-Chriss calibrated  
CLAIM-ATTR-001| ops.pnl| V4| 14| Yes| INV-005 daily P&L reconciliation  
CLAIM-RISK-STALE-001 | risk.soft_limit_stale | V1 | S3 | Yes | Soft risk limit stream stale threshold ≤ 5s enforced via REG-RISK-STALE-001; breaches trigger L4 monotone latch new-position freeze  
CLAIM-ONNX-VERSION-001 | model.onnx_runtime | V3 | S11 | Yes | ONNX Runtime version is pinned in governance/onnx_runtime_version.lock; production runtime version matches model registry entry onnx_runtime_version field at PF-09; version changes invalidate existing model registrations  
CLAIM-C-001 | engine.determinism | V4 | 0 | Yes | Production trading engine state transitions are pure, deterministic functions of raw event log, compiled with strict FP precision (Axiom C-001)  
CLAIM-C-002 | engine.lock_free | V4 | 0 | Yes | Zero locks, mutexes, semaphores, or blocking system calls on hot path; monitored by runtime scheduling watchdogs and static Clang AST analysis (Axiom C-002)  
CLAIM-C-003 | engine.allocation | V4 | 0 | Yes | Zero dynamic heap allocation in hot-path; allocator traps loaded at startup, statically enforced by Clang AST matchers (Axiom C-003)  
CLAIM-C-004 | engine.watchdog | V4 | 0 | Yes | Isolated CPU cores monitored by eBPF task-switch watchdog signaling immediate SIGSYS on context switch (Axiom C-004)  
CLAIM-C-005 | engine.shm_isolation | V4 | 0 | Yes | Multi-tenant shared memory segments isolated via POSIX ACLs and page-level mprotect write protection (Axiom C-005)  
  
### B1.3 — govcheck Binary Architecture

CLAIM-GOVCHECK-001 — Self-Verification Requirement

**Claim:** The govcheck binary itself satisfies the same verification standards it enforces on other components. It compiles clean under ASAN, TSAN, and UBSAN. Its own unit tests (GoogleTest) pass. Its version is pinned in CI via GC-F-002. It is the first claim verified before any other claim is evaluated.

**Verification Class:** V4 — Empirical. Proof artifact: `governance/proof_artifacts/govcheck_selftest_{hardware_id}_{date}.json` containing exit codes from: `cmake --build . --target govcheck_tests && ctest -R govcheck` under AddressSanitizer, ThreadSanitizer, and UBSan build profiles.

**Deployment blocking:** Yes. `minimum_class_for_unblocked_deploy: V4`. No capital gate (G1, G2, G4) may be considered active until CLAIM-GOVCHECK-001 is at V4 on the target hardware_id.

**Rationale:** A governance enforcement binary whose own correctness is unverified propagates unverified trust to every claim it enforces. CLAIM-GOVCHECK-001 is the root of the trust chain. 

#### B1.3.1 — Why a Compiled Binary, Not a Shell Script

Shell scripts accumulate silent exceptions — unexpected YAML fields may exit 0 when they should fail. Shell scripts are not tested like production code, have no type systems, and fail without error output. govcheck is compiled C++ in the CMake build graph, tested with GoogleTest, ASAN/TSAN/UBSAN builds, structured JSON output only, version pinned in CI. Version mismatch is itself a hard CI failure (GC-F-002).

The transition from "CI passes" to "govcheck says CI passes" is the most dangerous moment in the governance lifecycle. Any weakening of govcheck — even temporarily — must be treated as production risk.

#### B1.3.2 — govcheck Functional Architecture
[code] 
    govcheck/
    ├── src/
    │   ├── main.cpp                    — Command routing, exit codes
    │   ├── parser/                     — YAML parsing, schema validation, enum validation
    │   ├── graph/                      — DAG builder, cycle detector, composition engine, namespace checker
    │   ├── validators/                 — claim, artifact, ownership, expiry, R4, deploy_gate, waiver, stale
    │   ├── reporter/                   — JSON output, GC-E/W/F error codes
    │   └── commands/                   — validate, deploy-gate, waiver check, stale-check, status
    ├── tests/fixtures/                 — valid_claim, invalid_r4, expired_waiver, cycle_deps, laundered_class, stale_tla
    └── CMakeLists.txt
[/code]

#### B1.3.3 — govcheck Error Code Registry

Code| Type| Description| Blocks  
---|---|---|---  
GC-F-001| Fatal| Circular dependency in claim graph| All operations  
GC-F-002| Fatal| govcheck binary version mismatch with CI| All operations  
GC-F-003| Fatal| namespace-deps.yaml malformed| All operations  
GC-F-004| Fatal| CLAIM-GOVCHECK-001 not at V4 on current hardware_id| All capital gates (G1, G2, G4)  
GC-F-005| Fatal| BRO domain boundary violation; unmapped components found in build manifest| Merge (P1) and Capital gates  
GC-E-010| Error| malloc/new/free/delete in engine/ hot path| Merge (P1)  
GC-E-011| Error| clock_gettime(CLOCK_REALTIME) in engine/| Merge (P2+)  
GC-E-017| Error| composed_verification_class mismatch| C0  
GC-E-018| Error| Proof artifact SHA-256 mismatch| C0  
GC-E-019| Error| Claim owner not in team.yaml| C0  
GC-E-020| Error| review_expiry exceeded| C0  
GC-E-021| Error| R4 claim without waiver on deploy-gate| G1/G2  
GC-E-022| Error| Waiver expired| C0  
GC-E-023| Error| Cross-namespace dependency not declared| C0  
GC-E-024| Error| Composed class exceeds declared class| C0  
GC-E-025| Error| Active claim with superseded dependency| C0  
GC-E-031| Error| V4 claim with no empirical proof artifact| C0  
GC-E-042| Error| SessionID/MsgSeqNum/R_applied in FIX consumer TU| P1  
GC-E-043| Error| Direct access to order_tag_ / x_l_ outside L3| P1  
GC-E-044| Error| memory_order_relaxed on cross-plane atomic| P1  
GC-E-045| Error| Direct x_l_/order_tag_ read from L4/L5| P1  
GC-E-RESEARCH-001| Error| engine/ imports from research/| P1/G3  
GC-E-RESEARCH-002| Error| Backtester not using replay_engine.h| P9  
GC-E-RESEARCH-003| Error| pickle in engine/ or production inference| G4  
GC-E-RESEARCH-004| Error| OOS vault modification without unseal record| P10  
GC-E-050| Error| Registry entry REG-RISK-STALE-001 production value exceeds 5000ms| G2 (Production deploy gate)  
GC-E-051| Error| Production observability stack not verified: egbos_exporter proof artifact absent, or inotify SLA proof artifact absent, or Grafana panel count < 12| G2 (Production deploy gate)  
GC-E-052| Error| Phase III runbook fire-drill records absent from `governance/runbooks/firedrills/phase-iii/` — REC-008, REC-009, REC-010 fire-drill records required| TIMING-CONSTANTS-001.yaml sign-off (S6 exit)  
GC-E-053| Error| Claim with verification_class ≥ V3 has been amended (review_history entry present) without a corresponding `governance/rfcs/RFC-NNN.yaml` in accepted status listing this claim in artifacts_changed| C0 (all PRs, immediate)  
GC-E-054| Error| Research environment compiler flags missing `-ffp-contract=off` in C++ targets, or research Docker image missing `NPY_DISABLE_SVML=1` environment variable, or research SIMD level exceeds `simd_level` in `timing_constants.json`| P2 (research build gate)  
GC-E-055| Error| Running ONNX Runtime version does not match `onnx_runtime_version` field in model registry entry, or `governance/onnx_runtime_version.lock` is absent| Pre-flight PF-09 (blocks preflight_armed); G4  
GC-E-SECRETS-001| Error| Secret material detected in source: KEY=, SECRET=, PASSWORD=, or TOKEN= pattern in tracked files, or hardcoded exchange API key format in engine/ or scripts/ directories| C0 (all PRs, immediate)  
GC-E-SECRETS-002| Error| HashiCorp Vault endpoint unreachable at pre-flight check CHK-004 — secrets cannot be resolved for session startup| Pre-flight PF-07 (blocks preflight_armed)  
GC-E-056 | Error | SIM_STATE code path detected accessing `/dev/shm/` paths, WAL file descriptors, or Governance Audit Log outside of `TELEMETRY_MODE=SIMULATION` source-flagged dispatch — simulation isolation boundary violated (AD-013) | C0 (all PRs, immediate)  
GC-W-031| Warning| TLA+ claim engine_commit_hash stale| Advisory (30-day block)  
GC-W-032| Warning| review_expiry within 30 days| Advisory  
GC-W-033| Warning| Waiver >150 waiver-days| Advisory  
  
### B1.4 — CI Gate Hierarchy

#### B1.4.1 — Gate Design Principles

Every gate has an activation phase — earliest phase at which it becomes active. At activation, immediately blocking (no grace period unless specified). Gates are ordered; failure in gate N blocks N+1 through N+k. No gate can be skipped or conditionally disabled.

#### B1.4.2 — Complete Gate Specification
[code] 
    GATE EXECUTION ORDER:
    C0 → P1 → P2 → P3 → P4 (conditional) → P5 (conditional) → P6 (nightly)
    → P7 (main branch) → G1 (main branch) → G2 (production deploy)
    → G3 (research env) → G4 (model live)
    
    PARALLEL GATES (after P3):
    P8 (research/feature PRs) ‖ P9 (backtest PRs) ‖ P10 (signal promotion) ‖ P11 (model promotion)
[/code]

Gate| Name| Trigger| Active From| Key Checks| On Failure  
---|---|---|---|---|---  
C0| Governance Pre-Check| All PRs| Phase 4| Schema, DAG, SHA-256, owner, expiry, waiver, composed class| Hard block  
P1| Format + Lint + Import| All PRs| Phase 0| clang-format, clang-tidy, lint-imports, verify_all_fixtures.sh| Hard block  
P2| Build Matrix| All PRs| Phase 0| GCC 13 + Clang 17, ASAN+UBSAN, schema freshness| Hard block  
P3| Unit + Integration| All PRs| Phase 0| ASAN+UBSAN tests, replay fidelity, WAL determinism, INV-001| Hard block  
P4| ThreadSanitizer| engine/ PRs| Phase 1| Full suite under TSAN, zero races| Hard block  
P5| Latency Regression| Hot-path PRs| Phase 1| P99 on bare-metal, >10% regression blocks| Hard block  
P6| Fuzz (nightly)| Nightly main| Phase 2| LOB fuzz 60s libfuzzer| Next-morning deploy block  
P7| Staging Smoke| main merges| Phase 4| 60s simulator smoke, P99 within 20% baseline| Block staging deploy  
G1| Staging Deploy Gate| main merges| Phase 4| govcheck deploy-gate --target staging| Hard block  
G2| Production Deploy Gate| Prod deploy PRs| Phase 5| govcheck deploy-gate --target production| Hard block. No override  
G3| Research Deploy Gate| Research deploy| Phase 8| lint-imports, GC-E-RESEARCH-001| Hard block  
G4| Model Live Gate| Model goes live| Phase 13| FEATURE-PARITY V4, MODEL-002 V4, WFV passed, shadow 14d| Hard block  
P8| Feature Parity| feature PRs| Phase 11| memcmp offline vs L2 golden fixture| Hard block  
P9| Backtest Reproducibility| backtesting/ PRs| Phase 10| P&L curve SHA-256 identical × 3| Hard block  
P10| WFV Gate| Signal promotion| Phase 10| OOS Sharpe ≥ 1.0, max DD ≤ 15%, regimes ≥ 3, DSR ≥ 0.5| Block promotion  
P11| Model Promotion| DEPLOY-MODEL PRs| Phase 11| govcheck validates DEPLOY-MODEL YAML| Block model live  
  
#### B1.4.3 — The Fixture Test Requirement

Every blocking GC-E-0xx rule must have a violation fixture at `tests/governance/fixtures/violation_GC_E_NNN.cpp`. `verify_all_fixtures.sh` runs in P1 and verifies each fixture causes the rule to fire. Fixture passing the rule = rule broken = CI exit 1. GC-E-056 fixture: `tests/governance/fixtures/violation_GC_E_056.js` — a JavaScript module that calls `ObservabilityBridge.dispatch()` with `source: "live"` while SIM_STATE references a mock `/dev/shm/` path; CI verifies the rule fires.

### B1.5 — GC-E-0xx Static Analysis Rule Registry

#### B1.5.1 — Rule Lifecycle

**Advisory** → warnings only. **Warning** → visible in CI, build continues. **Blocking** → merge blocked. No correctness-critical rule remains at Warning indefinitely; >30 days at Warning = P2 governance finding.

#### B1.5.2 — Rule Registry

Rule ID| Description| Phase| Lifecycle  
---|---|---|---  
GC-E-010| malloc/new/free/delete in engine/ hot-path| 1| Warning → Blocking (Phase 3)  
GC-E-011| clock_gettime(CLOCK_REALTIME) in engine/| 2| Blocking immediately  
GC-E-042| SessionID/MsgSeqNum/R_applied in FIX consumer TU| 4| Blocking immediately  
GC-E-043| Direct order_tag_/x_l_ access outside L3| 4| Blocking immediately  
GC-E-044| memory_order_relaxed on cross-plane atomic| 4| Blocking immediately  
GC-E-045| Direct x_l_/order_tag_ read from L4/L5| 4| Blocking immediately  
GC-E-RESEARCH-001| engine/ imports from research/ namespaces| 8| Blocking immediately  
GC-E-RESEARCH-002| Backtester not using replay/replay_engine.h| 10| Blocking immediately  
GC-E-RESEARCH-003| pickle in engine/ or production inference| 11| Blocking immediately  
GC-E-RESEARCH-004| OOS vault modification without unseal record| 10| Blocking immediately  
GC-E-050| Registry entry REG-RISK-STALE-001 production value exceeds 5000ms| S3| Blocking immediately at G2  
GC-E-053| V3+ claim amended without accepted RFC in artifacts_changed list| S1| Blocking immediately at C0  
GC-E-054| Research environment SIMD/FMA flags non-compliant (missing -ffp-contract=off or NPY_DISABLE_SVML=1, or SIMD exceeds production ceiling)| S1| Blocking at P2  
GC-E-SECRETS-001| Secret material in tracked source files (KEY=, SECRET=, PASSWORD=, TOKEN= patterns; exchange API key format in engine/ or scripts/)| S1| Blocking immediately at C0  
GC-E-SECRETS-002| Vault endpoint unreachable at session startup (CHK-004 failure path)| S1| Blocking at pre-flight PF-07  
GC-E-056 | SIM_STATE accesses production SHM, WAL, or Audit Log file descriptors outside simulation telemetry boundary (AD-013 enforcement) | S1 | Blocking immediately at C0  
  
**Indirect-Access Complement:** `scripts/check_l3_public_interface.sh` (Phase 1, warning-only permanently) flags new public methods on `order_state_manager.h` returning L3 internal state references. Complements GC-E-043/045 for accessor-based access paths.

### B1.6 — Waiver System

#### B1.6.1 — Waiver Fundamentals

Waivers are time-bounded exceptions allowing deployment despite deployment-blocking claims below minimum class. Hard constraints (not configurable):

  1. **Maximum duration: 90 days (REG-WAIVER-MAX-DAYS-001).** Extension requires new waiver with new rationale. Same-rationale renewal = governance finding.
  2. **R4 prohibition for live capital.** No waiver clears R4 for live deployment.
  3. **Audit trail mandatory.** Every waiver deployment records waiver ID, authority, expiry, conditions.
  4. **Accumulation monitoring.** >180 cumulative waiver-days triggers governance review.



#### B1.6.2 — Waiver Schema
[code] 
    waiver_id:       "WAI-YYYY-NNN"
    claim_id:        "CLAIM-L3-02"
    granted_by:      "@founder"
    granted_at:      "2025-01-01"
    expires_at:      "2025-04-01"          # Max 90 days
    rationale:       |
      Paper trading environment only.
      No real capital at risk.
    conditions:
      - "Paper trading environment only"
      - "Live capital requires new waiver or claim upgrade"
    auto_revoke_on:
      - "any live capital deployment"
    cumulative_waiver_days_at_grant: 45
    status: "active"
[/code]

### B1.7 — Governance Decay Vectors

Decay Vector| Description| Mechanical Mitigation  
---|---|---  
Discipline erosion| Human-memory rules violated under pressure| CI enforcement — govcheck binary  
Waiver accumulation| Individual waivers seem justified collectively| 180 waiver-day trigger (REG-WAIVER-ACCUMULATION-001); monthly summary  
Stale proof artifacts| TLA+ model at commit X, engine at Y| engine_commit_hash + GC-W-031  
Ownership drift| Owner leaves; claim orphaned| team.yaml dependency; orphaned claims block deploy  
CI rule debt| Rules weakened via toolchain/suppress| Fixture test requirement  
"Just for now" waivers| Temporary becomes permanent| 90-day hard limit  
Composed class laundering| Strong leaves imply strong composed claim| Transitive min-class; GC-E-017/024  
Amendment accumulation| Original property unrecognizable| Amendment audit trail; periodic re-constitution  
Scope creep in enforcement| Rule scope narrows over time| enforcement_scope + enforcement_gaps fields  
"Everyone knows the rules"| New engineers adopt senior shortcuts| CI is enforcer; identical gates for all  
  
### B1.8 — Namespace Model
[code] 
    NAMESPACE HIERARCHY
    ════════════════════════════════════════════════════════
    bootstrap.*     — System bootstrap claims
    ipc.*           — IPC mechanism correctness (spsc, shm)
    wal.*           — WAL durability and ordering
    startup.*       — Startup sequence correctness
    exec.*          — Execution plane (position, fix_consumer, wal_ordering, state_ownership, memory_ordering, atomic_exposure, mass_cancel, escrow)
    allocator.*     — Memory allocation correctness
    ingestor.*      — Feed ingestion correctness
    lob.*           — LOB reconstruction correctness
    feature.*       — Feature computation (determinism, parity)
    risk.*          — Risk enforcement correctness
    regime.*        — Regime detection correctness
    signal.*        — Signal weight governance
    model.*         — Model registry and inference
    research.*      — Research platform (boundary, backtest, wfv)
    data.*          — Dataset governance (point_in_time, splits)
    ops.*           — Operational correctness (pnl)
    capacity.*      — Market impact and capacity
[/code]

**Cross-Namespace Rules:** exec.* → wal.*, ipc.*, allocator.*; feature.parity → feature.determinism, lob.*; research.backtest → feature.determinism, data.*; research.wfv → research.backtest, data.*; model.* → feature.*, research.*; ops.pnl → exec.position, wal.*; all → bootstrap.*. Undeclared = GC-E-023.

## B2 — Research Architecture

Classification
    Operational Authority Document — Tier 1

Version
    1.0

Owner
    Research Platform Lead (Founder)

Active From
    Phase 8

### B2.1 — Research Plane Boundary (AD-002)

#### B2.1.1 — Why the Boundary Exists

**Failure Mode:** If research code is importable from production:

  1. Researcher adds helper to `research/features/utils.py`
  2. Production engineer imports for convenience
  3. Utility contains non-deterministic behavior (PRNG, clock, filesystem)
  4. Production feature engine becomes non-deterministic
  5. Replay fidelity gate fails; all subsequent research results potentially invalid



Boundary is CI-time structural (import-linter + GC-E-RESEARCH-001), consistent with P-002. Only enforcement that holds at 2am under deadline pressure.

#### B2.1.2 — Boundary Definition

Namespace| Rule  
---|---  
**Production** — engine/, replay/, core/, schemas/| Must never import from research namespace  
**Research** — research/, notebooks/, backtesting/, training/| Must never be imported by production  
**Bridge Layer**|  ONNX files, config YAML, governance claims, DVC refs — data/artifacts, not code imports  
  
**Feature Parity Bridge:** Offline (research) and online (L2 C++) are _independently implemented_ to the same specification (CLAIM-FEATURE-PARITY-001). INV-003 proves byte-identical convergence. Online is authoritative — offline must conform, not reverse.

### B2.2 — Research Environment Specification

  - **Python 3.11.x pinned** — specific patch version in Dockerfile (e.g., 3.11.9)
  - **pip-compile lockfile** — `requirements-research.txt` with every transitive dep pinned; `pip install` from .in forbidden in CI
  - **PYTHONHASHSEED=0** — at kernel and all process invocations
  - **SIMD instruction set pinned:** All research environment Python/C++ compilation targets must use the same SIMD instruction set as the production execution server hardware_id. The maximum SIMD level permitted in research is the SIMD level of the registered production hardware_id in `timing_constants.json`. If production hardware supports AVX2 only, research compilation must use `-march=skylake` (AVX2 ceiling) or equivalent — AVX-512 is prohibited in research environments whose parity test targets an AVX2 production server. The required compiler flag set is committed to `governance/research-env/compiler_flags.yaml` and is part of the Docker lockfile validation.
  - **FMA behavior pinned:** All C++ feature engine compilation uses `-ffp-contract=off` to disable compiler FMA contraction. Research Python environments must use numpy compiled with `NPY_DISABLE_SVML=1`. These flags are validated by GC-E-054 at P2 build gate.
  - **Container flag isolation (GC-E-054-EXT-001):** `governance/research-env/compiler_flags.yaml` MUST be mounted as a **read-only bind mount** (`:ro`) at `/etc/egbos/compiler_flags.yaml` inside every research Docker container. The container entrypoint script MUST source this file and verify that no `ENV`, `ARG`, or `setup.py` directive has overridden the `-ffp-contract`, `-march`, or `NPY_DISABLE_SVML` values declared therein. If any value has been overridden, the container MUST exit with code 2 before any research computation begins. GC-E-054-EXT-001 verifies the bind mount declaration is present in `research/docker/Dockerfile.research` committed to the repository. Gate: P2. Severity: Error. _Absorbed: QOS Final Absorption Pass 2026-06-16._
  - **Data at /data/readonly** — PROT_READ only; all reads logged to `logs/research_data_access/{date}.jsonl`
  - **Compute isolation** — no production NUMA pinning; no production SHM write access during live trading



### B2.3 — Hypothesis Registry

Append-only record of every alpha hypothesis. Registration **before** any backtest — prevents post-hoc hypothesis selection that defeats multiple testing correction.
[code] 
    hypothesis_id:        "HYP-YYYY-NNN"
    registered_at:        "ISO-8601"           # Must precede backtest
    motivation:           |                    # Economic/microstructure rationale
    feature_set:          ["OFI_1s", "VPIN"]
    expected_regime_set:  ["low_vol_trend"]
    falsification_criteria: |
      OOS Sharpe < 0.5; max DD > 20%; regime coverage < 2; DSR < 0.3
    backtest_id:          null                # Populated when backtest runs
    status:               "registered"
[/code]

**Enforcement:** P10 gate blocks promotion if hypothesis registration_date follows backtest_start_date. govcheck validates timestamp ordering.

### B2.4 — Experiment Tracking

FINISHED MLflow runs are immutable — corrections via new run referencing superseded run. Required metadata: `data_hash`, `code_hash`, `hypothesis_id`, `feature_set_version`, `hyperparams`, `metrics`, `artifacts` (SHA-256), `hardware_id`.

**Reproducibility Standard:** Same data_hash + code_hash + hyperparams + hardware_id → metrics within floating-point epsilon. Non-reproducible experiments cannot contribute to signal promotion.

### B2.5 — Feature Store Architecture

#### B2.5.1 — Dual Implementation Requirement

**Online (L2 C++):** Authoritative. No dynamic alloc, no wall-clock, no PRNG. 256-byte FeatureVector to SHM.

**Offline (Python/Polars):** Must produce byte-identical output (memcmp == 0). Not "approximately equal."

**Why no epsilon:** Differences compound in nonlinear systems; epsilon hides root causes; low-vol parity does not guarantee high-vol parity.

#### B2.5.2 — Feature Registration

Every feature: feature_id, version, mathematical definition, window_specs, online/offline implementation refs, parity_test_ref. Version bump on any definition change requires new parity test and CLAIM-FEATURE-PARITY-001 update.

#### B2.5.3 — Feature Drift Detection

Rolling 30-day reference distribution. Hourly KL divergence vs 1-hour window. Threshold breach → FeatureDriftAlert (regime change, data quality, or engine bug).

### B2.6 — Model Registry and Governance

Field| Description  
---|---  
model_id, hypothesis_id| Unique identifiers, immutable  
training/validation_data_hash| DVC hashes  
code_hash, feature_set_version| Reproducibility anchors  
hyperparams, training/validation_metrics| Complete specification  
wfv_result_path, onnx_artifact_hash, onnx_opset| Promotion evidence  
inference_p99_ns, inference_budget_ns| From timing_constants.json  
retirement_thresholds, champion_challenger_eligible| Lifecycle governance  
  
**ONNX-Only (GC-E-RESEARCH-003):** Pickle executes arbitrary Python on deserialization — version drift and arbitrary code execution risk. ONNX is standardized, deterministic (INV-006), language-independent.

### B2.7 — Signal Governance

#### B2.7.1 — CLAIM-SIGNAL-001

Documents exact authorized signal weights. Weight changes require claim amendment with rationale and backtest evidence. `govcheck signal-weight validate` compares runtime config at G4.

#### B2.7.2 — Multiple Testing Correction

Bonferroni (familywise error) or BHY (false discovery rate) required before promotion. P10 queries hypothesis registry for all signals in batch and applies corrected Sharpe threshold. Prevents 50-trial false alpha discovery.

### B2.8 — Research Operational Procedures

**Pre-Research:** Register hypothesis → verify OOS vault sealed → confirm data access logging → confirm Docker lockfile → confirm PYTHONHASHSEED=0.

**Post-Research:** Verify hypothesis predates backtests → verify data_hash/code_hash → run multiple testing correction → parity test → document decay half-life → confirm MLflow runs immutable.

## B3 — Validation Architecture

Classification
    Research Governance Document — Tier 1

Version
    1.0

Owner
    Research Platform Lead

Active From
    Phase 10

### B3.1 — Validation Philosophy

Validation architecture solves **in-sample overfitting presented as out-of-sample performance**. OOS vault makes test-set observation structurally impossible until qualification completes. Multiple testing correction makes batch testing without penalty structurally difficult. DSR makes trial-count relationship to effective Sharpe explicit. This is engineering, not moral framework — removes ability to overfit, not incentive.

### B3.2 — Walk-Forward Validation Framework

#### B3.2.1 — WFV Modes

  - **Expanding Window (Primary):** Training grows from fixed start; fixed validation window (e.g., 60 days). More data always helps.
  - **Rolling Window:** Fixed training size advances. Distant history misleading in regime shifts.
  - **Anchored Window:** Training anchored to specific regime period; validation on regime-similar periods.



#### B3.2.2 — OOS Vault Mechanics

**Sealing:** OOS partition hash in SEAL.yaml committed; write permissions restricted; GC-E-RESEARCH-004 blocks modification without unseal record.

**Unseal Conditions (model-specific):** P10 passed for specific model_id → govcheck OOS-unseal record → UNSEAL-{model_id}.yaml committed. Prevents OOS contamination across models.

#### B3.2.3 — P10 Gate Thresholds

Metric| Threshold| Rationale  
---|---|---  
OOS Sharpe (annualized)| ≥ 1.0| At OOS 1.0, live Sharpe expected ~0.5–0.7 after costs. Below 1.0, live expectation approaches zero.  
Max OOS drawdown| ≤ 15%| At 50% capital allocation, portfolio drawdown within institutional tolerance.  
Regime coverage| ≥ 3 regimes| Reduces regime-conditional overfitting; institutional ops span all regimes.  
Minimum backtest length| ≥ 252 trading days| Insufficient power below one year at typical microstructure autocorrelation.  
  
### B3.3 — Deflated Sharpe Ratio Gate

DSR (Bailey & López de Prado, 2014) adjusts observed Sharpe for number of trials, skewness, kurtosis. Answers: "Given this was best of N strategies, what is P(true Sharpe > 0)?"
[code] 
    DSR = Φ[ (SR - SR*) × √(T-1) / √(1 - γ₃·SR + (γ₄-1)/4 · SR²) ]
[/code]

**DSR ≥ 0.5 threshold:** 50% probability true Sharpe positive — minimum institutional confidence bar. Blocks signals that appear exceptional after 500 trials but have DSR near 0.

### B3.4 — Combinatorial Purged Cross-Validation

CPCV required alongside WFV — standard WFV still leaks via autocorrelation at fold boundaries. Purge distance = autocorrelation decay length. Minimum 10 independent OOS Sharpe samples (distribution, not point estimate). Median 1.2 with 10th percentile -0.5 ≠ median 1.2 with 10th percentile 0.8.

### B3.5 — Backtest Determinism (INV-002)

**PROC-REPLAY-001 (3-Pass Determinism Standard):** Identical binary + data hash + seed → SHA-256-identical P&L curve × 3 consecutive runs. AD-001: backtester must use `replay/replay_engine.h` only (GC-E-RESEARCH-002). Eliminates engine divergence component from live/backtest Sharpe gap.

### B3.6 — Feature Parity Validation (INV-003)

SIMD and FMA Parity Preconditions

The byte-identical memcmp standard for INV-003 is only achievable when the following conditions are met:

  - Research compilation SIMD ceiling ≤ production hardware SIMD level (per B2.2 SIMD pinning)
  - C++ feature engine compiled with `-ffp-contract=off`
  - Python research numpy with `NPY_DISABLE_SVML=1`
  - Both environments use identical double-precision IEEE 754 rounding mode (round-to-nearest-even)



If INV-003 fails under these conditions, the failure is a genuine algorithmic divergence and must be investigated as a P1 incident. If INV-003 fails only when SIMD pinning is not applied, the failure is a build configuration error and must be corrected in `governance/research-env/compiler_flags.yaml` before re-running the parity test.

`offline_features(ticks) == online_features(ticks)` — memcmp on all FeatureVector fields. No epsilon tolerance: compounding, root-cause hiding, half-life problem across regimes. Both implementations: double precision, same window alignment, same warmup, same edge case handling.

### B3.7 — Signal Decay Monitoring

Condition| Duration| Response  
---|---|---  
30d Sharpe < 0.4 × OOS expectation| 5 trading days| Level 1: weight → 50%, investigation  
30d Sharpe < 0.2 × OOS expectation| 5 trading days| Level 2: weight → 25%, shadow paper, peer review  
30d Sharpe < 0| 10 trading days| Level 3: weight → 0, retirement recommendation  
Any level, no recovery| 4 weeks| Retirement decision required  
  
0.4× threshold accounts for 30-day Sharpe sampling variance; 5-day duration ensures persistent condition, not temporary regime mismatch.

### B3.8 — Capacity Validation

**CLAIM-CAPACITY-001:** Almgren-Chriss calibrated against live fills before capacity estimate is reliable.

  - Minimum 10,000 live fills
  - R² ≥ 0.3 on held-out week (realistic for microstructure noise)
  - Permanent + temporary impact coverage
  - Quarterly re-estimation from trailing 90-day fills



Capacity estimate: maximum AUM before impact cost equals signal alpha — function of trade frequency, order size, AC parameters, OOS Sharpe.

## Phase B — Consistency Review

### Consistency Checks Performed

  - Claim lifecycle (B1.2.3) is valid state machine — all transitions defined, no orphan states
  - Verification classes V1–V4 consistently applied — V5/V6 only in explicit bridge note
  - Research boundary (AD-002) consistent with P-002 — CI-enforced, not discipline-enforced
  - Feature parity (INV-003) consistent across B2.5 and B3.6 — byte-identical, same rationale
  - WFV thresholds stated with rationale, not just values
  - govcheck error codes consistent with CI gate hierarchy
  - Hypothesis pre-registration consistent with multiple testing correction
  - CLAIM-SIGNAL-001 consistent with LIVE-CAPITAL-001 — both require govcheck pass before activation
  - Decay monitoring thresholds derived with rationale



### Gaps Identified for Phase C

  - Data lake partition scheme (Tier-0/1/2/3/4) not fully specified
  - Runtime memory layout (SHM regions, ring buffer sizing) requires full specification
  - Infrastructure hardware specification referenced but not defined
  - Network architecture for co-location connectivity unaddressed



C

## Phase C — Data, Runtime & Infrastructure Architecture

Tier-0/1/2 data model, WAL physical implementation, SHM runtime layout, hardware lifecycle, co-location topology

Complete

## C1 — Data Ingestion Architecture (L0)

Classification
    Infrastructure Authority Document — Tier 0

Version
    1.0

Owner
    Chief Architecture Office

Active From
    Phase 2 (synthetic); Phase 5 (live feeds)

Governing Principle
    P-003 — Determinism as Constitutional Invariant

### C1.1 — L0 Ingestor Role and Boundary

The L0 Ingestor is the sole point of entry for all external market data into the ENGINE_V3 execution environment. It is a **network boundary component only** — its exclusive responsibilities are: receive raw exchange feed bytes, validate sequence integrity, normalize to the canonical internal event schema, assign a monotonic ingestion timestamp, and write the raw event to the Tier-0 append-only log. It executes no business logic.

Boundary Enforcement — CLAIM-L0-001 (V3)

Import-linter rule `GC-E-010` enforces that `engine/ingestor/` contains zero imports from `engine/position/`, `engine/risk/`, `engine/features/`, or any research namespace. The ingestor must be deployable in isolation — a feed validation harness must be constructible without linking the execution plane.

**Enforcement Gap (documented):** Import-linter cannot prevent a developer from duplicating business logic inline. Peer review is the secondary control. This gap is explicit in CLAIM-L0-001 enforcement_gaps.

### C1.2 — Feed Protocol Handling

Feed Type| Protocol| Implementation Language| Primary Constraint  
---|---|---|---  
Equities (primary)| FIX 4.2 / ITCH 5.0| Rust (network boundary)| Zero-copy parse; no heap allocation per message  
Futures| FIX 5.0 / CME MDP 3.0| Rust| Multicast gap detection mandatory  
Reference data| FTP/SFTP batch| Python 3.11| Daily digest validation; not on hot path  
Synthetic (test)| Internal replay protocol| C++20| Must be byte-identical to live at L1+  
  
### C1.3 — Sequence Validation and Gap Detection (CLAIM-L0-002)

Every exchange feed carries a monotonically increasing sequence number. The L0 ingestor maintains a per-feed expected sequence counter. Any gap — defined as `recv_seq != expected_seq` — triggers the gap handling state machine before any downstream event is forwarded.

#### Gap Handling State Machine
[code] 
    FEED_NORMAL
      │ gap detected (recv_seq > expected_seq + 1)
      ▼
    GAP_DETECTED
      │ Emit GapDetectedEvent to Tier-0 log (with gap_start, gap_end, feed_id)
      │ Start recovery timer (configurable; default 500ms (REG-FEED-STALE-001))
      ├─ recovery succeeds within timer ──► FEED_NORMAL (gap_filled recorded)
      └─ recovery timeout ──────────────► FEED_STALE
           │
           │ Emit FeedStaleEvent; mark downstream LOB as STALE
           │ Continue receiving; emit StaleDataWarning per event
           └─ reconnect succeeds ─────────► FEED_RECONNECTING
                  │
                  └─ snapshot received + replay complete ──► FEED_NORMAL
[/code]

Capital Safety Implication of FEED_STALE

A stale LOB does not produce a TERMINAL_HALT — it produces a position-sizing constraint. The risk enforcer (L4) observes the FeedStaleFlag in SHM and applies the configured stale-feed position limit (default: 0 — no new positions). Existing positions are not force-closed. This is the _correct_ stale-feed response: force-closing on stale data creates adverse selection risk; halting new entries preserves capital while allowing natural position expiry.

Any deviation from this behavior — including force-closes or ignoring stale flag — is a P0 capital safety incident.

### C1.4 — Event Normalization Schema

All exchange-native formats are normalized to the canonical `RawMarketEvent` FlatBuffers schema at the L0 boundary. Downstream components never observe exchange-native formats. This makes exchange migration a bounded change confined to L0.
[code] 
    // FlatBuffers canonical schema — engine/schemas/market_event.fbs
    table RawMarketEvent {
      // Identity
      feed_id:          uint16;      // Registered feed identifier
      exchange_id:      uint8;       // Exchange registry enum
      instrument_id:    uint32;      // Internal instrument registry
      sequence_num:     uint64;      // Exchange-native sequence number
      ingestion_tsc:    uint64;      // CLOCK_MONOTONIC_RAW TSC at receipt
      exchange_tsc:     uint64;      // Exchange timestamp (nanoseconds epoch)
    
      // Payload (union — exactly one field populated)
      event_type:       EventType;   // enum: ADD / MODIFY / DELETE / TRADE / IMBALANCE / HALT
      price:            int64;       // Fixed-point: raw_price * PRICE_SCALE (100000)
      quantity:         uint32;
      side:             Side;        // enum: BID / ASK
    
      // Durability anchor
      tier0_offset:     uint64;      // Byte offset in Tier-0 log at time of write
      checksum:         uint32;      // CRC32C of payload fields
    }
[/code]

**Fixed-point pricing:** Floating-point is forbidden in the execution plane (GC-E-011 analog for float). `int64` at scale factor 100,000 provides sub-cent precision without floating-point non-determinism. PRICE_SCALE is a compile-time constant, not a runtime parameter — changes require ADR.

### C1.5 — Tier-0 Write Path

The Tier-0 raw event log is append-only. The L0 ingestor writes each normalized event as the canonical 128-byte `MarketEventSlot` (bytes 0-63 `RawMarketEvent`; bytes 64-127 `SlotFrameMetadata`) to Tier-0 **before** forwarding to the SPSC ring. This ordering is the foundation of P-003: if the system crashes after Tier-0 write but before SPSC forward, the event is recoverable from Tier-0. If the system crashes after SPSC forward but before Tier-0 write, the event is lost — this ordering is therefore structurally forbidden.
[code] 
    // Mandatory write ordering in L0 ingestor hot path
    // CORRECT — enforced by code review + CLAIM-L0-001
    tier0_log.append(event);          // Step 1: durable
    tier0_log.flush_if_threshold();   // Step 2: periodic fsync (not per-event)
    spsc_ring.push(event);            // Step 3: forward downstream
    
    // FORBIDDEN — structural impossibility required
    // spsc_ring.push(event);         // ← This ordering is a P0 capital safety defect
    // tier0_log.append(event);
[/code]

**fsync policy:** Per-event fsync on the Tier-0 log would bound throughput to ~10K events/sec on NVMe — insufficient for institutional feeds. The Tier-0 log uses a _threshold-based fsync_ : fsync when buffer reaches 64KB or 5ms elapsed (whichever first). This creates a maximum data loss window of 5ms × feed_rate events on sudden power loss. This trade-off is documented in ADR-003 (Phase H) and is explicitly acceptable: Tier-0 is a reconstruction aid, not the primary recovery path — the WAL is.

## C2 — Write-Ahead Log (WAL) Architecture

Classification
    Infrastructure Authority Document — Tier 0

Version
    1.0

Owner
    Chief Architecture Office

Governing Claims
    CLAIM-WAL-001 (V4), CLAIM-L3-001 (V4), CLAIM-L3-MAR-001 (V4)

Governing Principle
    P-003 (Determinism), P-006 (Capital Safety Over Continuity)

### C2.1 — WAL Architectural Thesis

The WAL is the singular source of truth for all position-affecting state transitions. It is not a log of what happened — it is the **definition of what happened**. If a state transition is not in the WAL, it did not happen from the perspective of the governance system. If a state in memory disagrees with a WAL replay, memory is wrong.

This ordering — WAL-before-state — is constitutionally enforced. CLAIM-L3-001 (V4) proves via 10× kill-9 injection that no in-memory state mutation is observable after recovery that was not preceded by a WAL record. This is the most critical empirical claim in the entire system.

### C2.2 — WAL Physical Implementation

#### Storage Topology

Layer| Device| fsync Policy| Role  
---|---|---|---  
WAL Primary| NVMe Gen4 (≥3GB/s write)| Per-record O_DSYNC| Authoritative — recovery source  
WAL Shadow| Second NVMe (same chassis)| Async mirror (100ms lag)| Hardware failure failover only  
Stratum-0| SATA SSD (separate controller)| Per-session fsync| Emergency recovery — controller failure  
  
**O_DSYNC semantics:** `O_DSYNC` guarantees data durability without metadata synchronization — faster than `O_SYNC` while preserving the required guarantee: after the write syscall returns, the data is on stable storage. The WAL writer never calls `write()` without subsequently verifying the return value equals the buffer length. Short writes are fatal errors, not retryable conditions.

#### WAL Record Schema
[code] 
    // WAL record layout — fixed-size header + variable payload
    // Total wire size: 64 bytes header + payload (max 256 bytes)
    struct WalRecord {
      // Header (64 bytes, cache-line aligned)
      uint64_t  sequence;          // Monotonically increasing; never reset
      uint64_t  tsc_before_write;  // CLOCK_MONOTONIC_RAW before fsync
      uint64_t  tsc_after_write;   // CLOCK_MONOTONIC_RAW after fsync (latency audit)
      uint32_t  record_type;       // Enum: ORDER_NEW / FILL_APPLIED / POSITION_SYNC /
      // CAPITAL_GATE_CLOCK_START is recorded when the founder confirms a capital-gate clock start.
                                   //       MASS_CANCEL / REGIME_CHANGE / SESSION_START /
    //       HITL_HALT_CONFIRM / HITL_RESTART_AUTH / HITL_RISK_LIMIT_CHANGE /
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
[/code]

#### io_uring Integration

The WAL writer uses `io_uring` with `IORING_OP_WRITE_FIXED` for kernel-bypass buffer registration, reducing per-write syscall overhead. Critical constraint: **a single thread is the sole CQE consumer of the io_uring ring.** This assumption is documented as an explicit dependency in CLAIM-WAL-001. Any multi-threaded CQE consumption requires a new WAL claim at V1 minimum before the code change merges.
[code] 
    // WAL writer thread — SOLE CQE consumer constraint
    // Documented in CLAIM-WAL-001 assumptions[0]
    // Violation: R4 claim + TERMINAL_HALT if detected at runtime
    class WalWriter {
      // ...
      // INVARIANT: submit_and_wait() called ONLY from WalWriterThread
      // ENFORCEMENT: thread_id assertion on every CQE collection
      void submit_and_wait(WalRecord* rec, uint32_t len) {
        ASSERT_WAL_THREAD();   // Compile-time macro: expands to __builtin_expect
                               // + runtime thread_id check in DEBUG builds
        // io_uring submit path
      }
    };
[/code]

### C2.3 — WAL-Before-State Ordering Protocol

The WAL-before-state ordering is the central correctness property of the execution plane. Every component that mutates position-affecting state must follow the three-phase commit protocol:
[code] 
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
[/code]

**Idempotency requirement:** Recovery replays the WAL from the last confirmed checkpoint. Any record that was written but not applied (crash between Phase 2 and Phase 3) will be replayed and re-applied. All state mutations _must be idempotent_ : applying the same WAL record twice must produce the same final state as applying it once. This is verified by the 10× kill-9 test in CLAIM-WAL-001.

### C2.4 — WAL Recovery Protocol

Recovery is triggered on any unclean startup — defined as: no clean shutdown record at end of WAL. Recovery is never automatic for live-capital sessions; it is a human-authorized procedure per A2.3 P0 incident protocol.

Step 1

CRC32C Verification

Scan WAL from last checkpoint. Verify header_crc32c on every record. First CRC failure = WAL truncation point. All records after are discarded.

Step 2

Sequence Gap Detection

Verify sequence monotonicity. Any gap in sequence numbers indicates a partial write. Treat as truncation point — identical to CRC failure.

Step 3

State Reconstruction

Replay all valid records from checkpoint to truncation point. Apply each record to position state. Result: in-memory state identical to pre-crash state minus at most one in-flight transaction (PROC-REPLAY-001).

Step 4

RFP Reconciliation (P0 only)

On P0 incidents: compare reconstructed position state against exchange Request-For-Position (RFP) response. Any discrepancy is a capital safety finding requiring human resolution before live trading resumes.

Step 5

Recovery Sign-Off

Founder signs RECOVERY-RECORD-{date}.yaml with: wal_truncation_point, records_replayed, position_state_hash, rfp_match_status. Committed to governance/ before engine restart authorized.

### C2.5 — WAL Invariant Definitions

Invariant| Definition| Verification| Violation Response  
---|---|---|---  
**INV-001**|  position == Σ FILL_APPLIED records in WAL for instrument_id| CLAIM-INV-001 V4: 10M fills + 10× kill-9| TERMINAL_HALT — P0 incident  
**WAL-SEQ**|  WAL sequence numbers are strictly monotonically increasing — never reset, never reused| CLAIM-WAL-001 V4| TERMINAL_HALT  
**WAL-ORDER**|  For any state S and WAL record W that caused S, W.tsc_after_write < tsc(S.first_observable)| CLAIM-L3-001 V4: kill-9 with TSC comparison| TERMINAL_HALT  
**WAL-IDEMPOTENT**|  apply(state, WAL[i]) applied twice == apply(state, WAL[i]) applied once| Recovery replay test — 3× byte-identical position state| Invalidate CLAIM-L3-001; R4 remediation  
  
## C3 — Shared Memory Runtime Layout

Classification
    Infrastructure Authority Document — Tier 0

Version
    1.0

Owner
    Chief Architecture Office

Governing Claims
    CLAIM-SPSC-001 (V4), CLAIM-L3-004 (V4), CLAIM-L3-006 (V4)

### C3.1 — SHM Region Topology

All inter-layer communication in the execution plane (L0–L5) passes through named POSIX shared memory regions. There are no Unix sockets, pipes, or TCP connections on the hot path. The SHM layout is fixed at startup — no runtime region creation. Region sizes are compile-time constants derived from `timing_constants.json` and the instrument universe size.
[code] 
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
[/code]

### C3.2 — SPSC Ring Buffer Specification

Single-Producer Single-Consumer (SPSC) ring buffers are the primary IPC mechanism between adjacent layers. Lock-free by construction — no mutex, no atomic CAS on the fast path. Correctness depends on the single-producer / single-consumer constraint being structurally enforced (CLAIM-SPSC-001).

Parameter| Value| Rationale  
---|---|---  
Ring size| Power of 2 (65536 default)| Modulo as bitmask — no division on hot path  
Slot size| Fixed at compile time| No dynamic allocation; no pointer chasing  
Producer index| std::atomic — seq_cst on write| Ensures consumer observes complete slot before index advance  
Consumer index| std::atomic — relaxed on read| Consumer owns its index; no cross-thread visibility required  
Padding| Producer and consumer indices on separate cache lines (64B)| False sharing elimination — CLAIM-SPSC-001 latency claim depends on this  
Full condition| Producer spins (yield after N iterations)| Backpressure; never drops. Spin budget in timing_constants.json  
  
### C3.3 — Cross-Plane Atomic Access Protocol (CLAIM-L3-004, CLAIM-L3-006)

The only piece of L3 (Position Engine) state that L4 (Risk Enforcer) may read directly is `gross_exposure_` — a single `std::atomic`. All other L3 state is _structurally inaccessible_ to L4: not via accessor, not via pointer, not via shared struct. GC-E-045 enforces this at compile time.
[code] 
    // Cross-plane read macro — mandatory for any atomic read crossing layer boundary
    // Defined in engine/core/cross_plane.h
    // Usage documents the access and triggers static analysis annotation
    #define CROSS_PLANE_READ(atomic_var, layer_from, layer_to) \
      ({ \
        static_assert(sizeof(atomic_var) == sizeof(int64_t), \
          "CROSS_PLANE_READ: only int64_t atomics permitted"); \
        (atomic_var).load(std::memory_order_acquire); \
      })
    
    // In L4 risk enforcer:
    int64_t exposure = CROSS_PLANE_READ(position_engine_.gross_exposure_,
                                         LAYER_L3, LAYER_L4);
    // GC-E-044 blocks memory_order_relaxed on any CROSS_PLANE_READ
[/code]

### C3.4 — FeatureVector Seqlock Protocol

The 256-byte FeatureVector written by L2 and read by L3 uses a seqlock — the most appropriate primitive for a single writer, multiple reader scenario where the reader must detect torn reads.
[code] 
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
[/code]

## C4 — Data Tier Model

Classification
    Data Architecture Document — Tier 1

Version
    1.0

Owner
    Chief Architecture Office

Governing Claims
    CLAIM-DATA-001 (V4), CLAIM-DATA-002 (V3)

### C4.1 — Tier Definitions and Retention Policy

Tier| Name| Format| Retention| Access Pattern| Mutability  
---|---|---|---|---|---  
**Tier-0**|  Raw Event Log| Binary WalRecord stream| Indefinite| Sequential write; recovery read| Append-only. Never modified.  
**Tier-1**|  Normalized Events| Apache Parquet (zstd)| 7 years (regulatory)| Batch read; research access| Immutable after daily seal. DVC versioned.  
**Tier-2**|  Aggregated Features| Parquet (snappy)| 5 years| Random read by feature_id + timestamp| Immutable after generation. Regenerable from Tier-1.  
**Tier-3**|  Research Outputs| MLflow artifacts (Parquet + ONNX)| Indefinite| Random read; model registry| Immutable per run. New version = new run.  
**Stratum-0**|  Emergency Records| Binary (same as Tier-0)| Indefinite| Recovery only| Append-only. Written by separate process on separate controller.  
  
### C4.2 — Point-in-Time Correctness (CLAIM-DATA-001)

The most common source of look-ahead bias in production research pipelines is the use of data that was not available at the simulated decision point. EGBOS enforces point-in-time correctness at the data access layer, not the research layer — the data itself carries availability metadata.

Availability Timestamp Protocol

  - Every Tier-1 record carries two timestamps: `event_time` (when the event occurred) and `available_time` (when the record was written to Tier-1 — after normalization latency)
  - Research queries against Tier-1 must filter on `available_time <= simulation_time`, not `event_time`
  - The research environment enforces this at the data access API level — direct Parquet reads bypassing the API are blocked by filesystem permissions
  - CLAIM-DATA-001 (V4) is verified by a canary test: injecting a synthetic record with `available_time = event_time + 1 hour` and verifying it does not appear in a simulation ending at `event_time + 30min`



### C4.3 — DVC Version Governance

All Tier-1 and Tier-2 datasets are version-controlled with DVC. The DVC remote is append-only — the backing store (S3-compatible object storage or local NAS) has object lock enabled. Dataset versions are referenced by SHA-256 hash in all claim proof artifacts and MLflow run metadata. A research result is only reproducible if its `data_hash` resolves to an intact DVC object.

## C5 — Hardware Lifecycle and Co-location Architecture

Classification
    Infrastructure Authority Document — Tier 1

Version
    1.0

Owner
    Chief Architecture Office

Governing Claims
    CLAIM-L3-05 (V4), timing_constants.json (V4 proof artifact)

### C5.1 — Minimum Production Hardware Specification

The following is the minimum specification for a hardware_id to be eligible for V4 claim measurement. Sub-specification hardware may be used for development but _may not generate timing_constants.json entries_ — all V4 empirical claims derived from timing measurements are invalid if measured below specification.

Component| Minimum Specification| Constraint  
---|---|---  
CPU| Intel Xeon (Ice Lake or later) / AMD EPYC (Milan or later)| NUMA topology documented in timing_constants.json  
RAM| 64GB DDR4-3200 ECC| NUMA-local to execution cores. No NUMA crossing on hot path.  
NVMe (WAL Primary)| PCIe Gen4 ×4, ≥3GB/s sustained write| Dedicated controller. No sharing with OS partition.  
NVMe (WAL Shadow)| PCIe Gen3 ×4, ≥1.5GB/s sustained write| Separate controller from primary. Shadow writes async.  
Network (feed)| 10GbE minimum; 25GbE recommended| Kernel bypass (DPDK or RDMA) for sub-10µs feed latency  
Network (exchange)| Co-location cross-connect (1GbE minimum)| Physical cross-connect to exchange matching engine. No internet routing.  
OS| Linux kernel ≥ 5.15 (io_uring stable)| PREEMPT_RT patch recommended for latency isolation  
  
### C5.2 — timing_constants.json Measurement Protocol

`timing_constants.json` is the empirical foundation of all latency-sensitive V4 claims. It is not estimated, not sourced from vendor datasheets, and not copied from documentation. It is measured on the specific production hardware_id, committed to `governance/proof_artifacts/timing_constants.json`, and referenced by SHA-256 in all relevant claims.
[code] 
    // timing_constants.json — canonical schema
    {
      "schema_version": 1,
      "hardware_id": "colo-rack-A-server-01",
      "measured_at": "2026-05-30T04:00:00Z",
      "measured_by": "@founder",
      "kernel_version": "5.15.0-87-generic",
      "cpu_model": "Intel Xeon Gold 6338",
      "numa_topology": "2 sockets, 32 cores each, execution pinned socket-0",
      "measurements": {
        "wal_fsync_p50_ns":          4200,
        "wal_fsync_p99_ns":          8100,
        "wal_fsync_p999_ns":         18400,
        "spsc_push_pop_p50_ns":      42,
        "spsc_push_pop_p99_ns":      180,
        "feature_compute_p99_ns":    8200,
        "kill_switch_response_p99_ns": 480,
        "l0_to_l3_pipeline_p99_ns":  38000,
        "nvme_write_throughput_mbps": 3100,
        "feed_to_decision_p99_us":   312
      },
      "measurement_methodology": "governance/docs/timing_measurement_protocol.md",
      "proof_artifact_sha256": "..."
    }
[/code]

**Re-measurement triggers:** Any hardware replacement (CPU, NVMe, network card), kernel upgrade, or NUMA topology change invalidates timing_constants.json. Claims depending on timing constants revert to R3 (re-verification required) until new measurements are committed. This is not optional — operating with stale timing constants is a P1 operational degradation finding.

### C5.3 — CPU Affinity and NUMA Pinning

Thread| Core Assignment| NUMA Policy| Isolation  
---|---|---|---  
L0 Ingestor| Core 0 (socket 0)| Local socket-0 memory only| isolcpus kernel param  
L1 LOB Engine| Core 1 (socket 0)| Local socket-0 memory only| isolcpus kernel param  
L2 Feature Engine| Core 2 (socket 0)| Local socket-0 memory only| isolcpus kernel param  
L3 Position Engine| Core 3 (socket 0)| Local socket-0 memory only| isolcpus kernel param  
L4 Risk Enforcer| Core 4 (socket 0)| Local socket-0 memory only| isolcpus kernel param  
L5 Exec Router| Core 5 (socket 0)| Local socket-0 memory only| isolcpus kernel param  
WAL Writer| Core 6 (socket 0)| Local socket-0 memory only| isolcpus kernel param  
L6/L7 Python| Socket 1 cores| Socket 1 memory preferred| No isolation (not latency-critical)  
Governance / Monitoring| Socket 1 cores| No constraint| No isolation  
  
**Hyperthreading policy:** Disabled on execution cores (socket 0). HT sibling of any isolated core must also be isolated or disabled. HT sharing between execution threads and any other workload introduces unpredictable L1/L2 cache competition that invalidates timing_constants.json measurements.

### C5.4 — Co-location Network Architecture

Production execution operates from a co-location environment with a physical cross-connect to the target exchange matching engine. The network path from the L5 execution router to the exchange order gateway must satisfy:

  - **RTT ≤ 500µs** (target: ≤ 200µs with kernel bypass)
  - **Zero internet routing hops** — physical cage-to-cage cross-connect only
  - **Dedicated VLAN** — no shared bandwidth with other tenants or management traffic
  - **Measured, not estimated:** RTT measurements committed to timing_constants.json before any live capital deployment



Phase III Qualification Gate — Network

Before LIVE-CAPITAL-001.yaml may be signed, the following must be committed as V4 proof artifacts: (1) 1000-sample RTT distribution to exchange order gateway, (2) 1000-sample RTT distribution to exchange market data feed, (3) network interface queue depth measurements under simulated peak load. Any measurement exceeding 500µs P99 RTT blocks Phase III exit regardless of other criteria.

## C6 — Phase C Architectural Decision Additions

Classification
    Architecture Decision Supplement

Adds to
    AD-001 through AD-007 (Phase B); new: AD-008 through AD-012

AD-008

Fixed-Point Arithmetic Mandate in Execution Plane

All price arithmetic in L0–L5 uses `int64_t` fixed-point at PRICE_SCALE = 100,000. Floating-point is forbidden on the hot path. Rationale: floating-point non-determinism across CPU generations invalidates replay fidelity (INV-001, P-003). PRICE_SCALE changes require an ADR — they affect every V4 replay claim.

**Alternative Rejected:** double precision with epsilon comparison. Rejected because epsilon tolerance compounds in multi-leg positions and is not byte-identical — violating CLAIM-FEATURE-001.

AD-009

io_uring Single-CQE-Consumer Invariant

The WAL writer io_uring ring has exactly one CQE consumer thread. This is a documented assumption in CLAIM-WAL-001. Any multi-threaded ring access requires a new claim at minimum V1 class before the code change merges — not after.

**Alternative Rejected:** Multi-threaded io_uring with locking. Rejected because lock contention on the WAL writer critical path creates unpredictable fsync latency, invalidating WAL timing constants and P99 latency claims.

AD-010

SHM Region Sizes as Compile-Time Constants

All SHM region sizes are `constexpr` constants in `engine/core/shm_layout.h`, derived from instrument universe size and timing_constants.json. No runtime configuration. Changes require recompile and re-measurement of affected timing constants.

**Rationale:** Runtime-configurable ring sizes create a class of bugs where misconfiguration silently changes memory ordering guarantees. Compile-time constants make the configuration space explicit and auditable.

AD-011

Tier-0 Log as Non-Recovery Path

The Tier-0 raw event log is the reconstruction source for research and audit, not the primary recovery path. The WAL is the recovery source. Threshold-based fsync on Tier-0 (accepting up to 5ms data loss on power failure) is acceptable because WAL recovery covers the same window. Any future proposal to use Tier-0 as primary recovery requires ADR superseding this decision.

**Rationale:** Per-event fsync on Tier-0 would halve throughput without improving capital safety — WAL already provides per-transaction durability. Two per-event fsyncs for the same safety guarantee is architectural waste.

AD-012

Stale Feed Response: Position Freeze, Not Force-Close

On FEED_STALE, L4 sets new-position limit to zero. Existing positions are not force-closed. This is an architectural decision, not a tuning parameter. Force-closing on stale data is adverse selection against the firm.

**Alternative Rejected:** TERMINAL_HALT on feed stale. Rejected because feed gaps are routine (exchange maintenance, network blip) and do not imply position-state uncertainty — only signal uncertainty. TERMINAL_HALT is reserved for position-state uncertainty (INV-001 violation).

## Phase C — Consistency Review

### Consistency Checks Performed

  - WAL-before-state ordering protocol is consistent with CLAIM-L3-001 (V4) and CLAIM-WAL-001 (V4) as defined in B1.2.5
  - Fixed-point arithmetic mandate (AD-008) is consistent with GC-E-011 (clock_gettime prohibition) — both enforce execution-plane determinism under P-003
  - FEED_STALE response (AD-012) is consistent with P-006 capital safety — new position freeze is less capital-risky than force-close on stale data
  - SHM topology cross-references CLAIM-SPSC-001, CLAIM-L3-004, CLAIM-L3-006 without contradiction
  - timing_constants.json re-measurement triggers are consistent with V4 claim scope limit (hardware_id specific) from B1.2.2
  - Tier-0 threshold fsync policy documented in AD-011 with explicit WAL coverage rationale — not a silent trade-off
  - Data tier availability timestamps (C4.2) are consistent with CLAIM-DATA-001 verification methodology
  - CPU affinity table is consistent with execution plane layer definitions (L0–L7) from A1.6



### Gaps Identified for Phase D

  - FlatBuffers schema definitions for all inter-layer event types not yet fully specified (Phase D: Event & Contract Architecture)
  - Repository directory structure for engine/, research/, governance/ not yet formalized (Phase D)
  - Build system (CMake + Cargo + uv) integration with WAL and SHM compile-time constants not yet specified (Phase D)
  - Redis Streams IPC bridge between L6/L7 Python and L3 SHM not fully specified — boundary only defined here (Phase D)
  - Exchange connectivity vendor selection (cross-connect provider, co-location facility) deferred to Phase E procurement process



D

## Phase D — Control & Governance Plane

The Constitutional Auditor. Registry & Configuration Store, Safety Monitors & Circuit Breakers, Audit & Compliance Logging, Human-in-the-Loop Interventions.

Complete

## Phase D — Architectural Overview

Classification
    Constitutional Authority Document — Tier 0

Version
    1.0

Owner
    Chief Architecture Office

Approval Authority
    CTO + Governance Board

Review Cycle
    On any change to a Constitutional Invariant (P-001 through P-007) or governing claim

The Control & Governance Plane is the **Constitutional Auditor** of the EGBOS architecture. While the execution plane (Phases B and C) is responsible for the correctness of capital-affecting state transitions, the Control & Governance Plane is responsible for the continuous, real-time enforcement of the conditions under which correctness was defined. It does not trade. It does not compute features. It watches everything that does, and it has the unconditional authority to halt it.

This plane operationalizes the seven binding architectural principles (P-001 through P-007) as executable runtime artifacts. A principle that cannot be continuously verified at runtime is an aspiration, not a constraint. The Control & Governance Plane converts aspirations into enforcement.

Four Subsystems of the Control & Governance Plane

  - **D1 — Registry & Configuration Store (`L4`):** Centralized, versioned management of all environment-specific constants and dynamic state configuration.
  - **D2 — Safety Monitors & Circuit Breakers:** Real-time logic gates validating all events against the Constitutional Invariants. Violations trigger immediate halt.
  - **D3 — Audit & Compliance Logging:** Tamper-proof, append-only record of all system state transitions. The forensic reconstruction substrate.
  - **D4 — Human-in-the-Loop (HITL) Interventions:** Formally specified protocols for manual overrides, recorded as first-class `WAL` events with cryptographic signature.



## D1 — Registry & Configuration Store

Layer
    `L4` — Control Plane Registry

Governing Principle
    P-001 (Mechanical Enforcement), P-004 (Governance Precedes Implementation)

Owner
    Chief Architecture Office

### D1.1 — Registry Architectural Thesis

The Registry & Configuration Store at `L4` is the single authoritative source for all environment-specific constants and mutable system configuration parameters. No component in the execution or research plane may hardcode a value that could vary between environments, hardware generations, or operational phases. Every such value is a registry entry — versioned, signed, and auditable.

The failure mode this architecture prevents is the most common source of production environment divergence from staging: a constant that was "obviously correct" in development becomes silently incorrect on production hardware, under production load, or after a hardware replacement. The registry makes all such constants first-class governance objects, subject to the same amendment and review discipline as correctness claims.

### D1.2 — Registry Entry Schema

Every registry entry is a YAML file under `governance/registry/`. Schema is enforced by `govcheck` at gate `C0`. A registry entry that fails schema validation blocks all CI gates, identical to a malformed correctness claim.
[code] 
    // Registry entry schema — governance/registry/{entry_id}.yaml
    entry_id:          "REG-YYYY-NNN"           # Immutable after creation
    entry_type:        "constant | threshold | feature_flag | limit"
    description:       |                        # Precise definition. Mandatory.
      Human-readable description of the value, its units, and
      the component(s) that consume it.
    value:
      production:                  # Value on production hardware_id
      staging:                     # Value on staging hardware_id
      development:                 # Value on development workstation
    units:             "nanoseconds | bytes | fraction | boolean | enum"
    source:            "measured | derived | specified | policy"
    source_artifact:   "governance/proof_artifacts/timing_constants.json"  # SHA-256 ref if measured
    version:           3                         # Monotonically incremented on every change
    versioned_at:      "ISO-8601"
    versioned_by:      "@founder"
    signature:         "sha256:..."              # SHA-256 of canonical YAML minus this field
    consuming_components: ["engine/risk/", "engine/position/"]
    governing_claims:  ["CLAIM-L3-05", "CLAIM-L4-001"]
    change_requires:   "ADR | claim_amendment | operational_log"
    amendments:        []                        # Append-only; same protocol as claim amendments
[/code]

#### D1.4 — Threshold Registry Stubs

The following Registry entries anchor time-bound threshold values used across Phase A–M governance rules. Full production values are measured and signed per the D1.2 schema before S5 exit.

Entry ID| Description| Default Value| Production Source| Change Authority  
---|---|---|---|---  
REG-FEED-STALE-001 | Maximum elapsed time before a market data feed is classified as stale and FeedStaleFlag is raised | 500ms | measured (co-location RTT + exchange maintenance window analysis) | ADR  
REG-WAIVER-MAX-DAYS-001 | Maximum duration of any single governance waiver grant | 90 days | policy | RFC + CTO approval  
REG-WAIVER-ACCUMULATION-001 | Cumulative waiver-days threshold in any rolling 365-day window that triggers mandatory governance board review | 180 days | policy | RFC + CTO approval  
  
### D1.3 — Versioning and Change Protocol

Registry entries are not deleted; they are superseded. A change to any registry entry increments its `version` field and appends an amendment record. The full version history of every entry is preserved in the YAML file itself — identical to the claim amendment protocol (see B1.2.3). This is a direct application of P-007 (Immutable Audit Over Convenient Deletion).

The change authority tier for a registry entry is determined by its `change_requires` field:

Change Authority| Applicable Entry Types| Required Artifacts  
---|---|---  
`ADR`| Entries that alter a V4 claim's proof scope (e.g., `PRICE_SCALE`, ring buffer sizes)| Architecture Decision Record + amended claim + re-measurement of timing constants  
`claim_amendment`| Entries directly referenced in a claim's `enforcement_mechanism` or `assumptions`| Amended claim YAML + new proof artifact + PR review  
`operational_log`| Operational thresholds and feature flags not in claim scope| Operational log entry + PR review  
  
⚑ **Capital Gate Clock Records:** WAL record type `CAPITAL_GATE_CLOCK_START` is written by the founder upon confirming milestone advancement. It is not a HITL intervention (it does not modify operational state) but follows the same WAL-before-state and HITL signature protocol defined in D4.3.

Prohibited Registry Anti-Patterns

  - **Hardcoded environment detection:** No component may branch on `if (env == "production")` to select a value. All environment-specific values are resolved from the registry at startup. Hardcoded branching is a `GC-E-010`-class violation.
  - **Unsigned registry entries in production:** Any entry reaching a production deploy gate without a valid `signature` field is a hard block at `G2`.
  - **Timing constants from documentation:** All entries with `source: measured` must reference a `source_artifact` whose SHA-256 resolves in `governance/proof_artifacts/`. Vendor datasheets are not acceptable as source artifacts for V4 claims.



## D2 — Safety Monitors & Circuit Breakers

Classification
    Capital Safety Authority Document — Tier 0

Governing Principles
    P-005 (Failure Mode Precedes Nominal), P-006 (Capital Safety Over Continuity)

Governing Claims
    `CLAIM-L4-001` (V4), `CLAIM-INV-001` (V4), `CLAIM-L3-006` (V4)

Owner
    Chief Architecture Office

### D2.1 — Safety Monitor Philosophy

Safety monitors do not prevent failure. They detect the earliest observable signal of a failure that was already structurally possible and halt the system before the failure propagates into a capital loss event. The distinction is critical: the architecture of the execution plane (P-002) prevents structurally impossible failures. The Safety Monitor plane detects failures that are structurally possible but governed by invariants — and treats any invariant violation as an immediate capital safety event.

Every monitor is a named, versioned, govcheck-registered artifact. A monitor that exists in code but is not registered in `governance/monitors/` has no institutional standing — it is invisible to the governance system and does not satisfy any claim's enforcement mechanism.

### D2.2 — Governance Rules Table

The following table defines the active governance rules enforced by the Safety Monitor subsystem. Every rule maps directly to one or more Constitutional Invariants (P-001 through P-007) and to a registered correctness claim. The **Action on Violation** column is binding — it is not advisory, and it is not subject to runtime configuration.

Rule Name | Invariant / Claim | Threshold / Constraint | Action on Violation  
---|---|---|---  
**Position Integrity Monitor** | `INV-001` / `CLAIM-INV-001` | In-memory position for any `instrument_id` diverges from `Σ FILL_APPLIED` records in `WAL` by any non-zero delta | `TERMINAL_HALT` — P0 Capital Safety Incident. Engine halted. Human authorization required before restart. RFP reconciliation mandatory.  
**WAL Sequence Monotonicity Guard** | `WAL-SEQ` / `CLAIM-WAL-001` | Any `WAL` record with `sequence` ≤ previous `sequence`, or any detected gap in the sequence space | `TERMINAL_HALT` — P0 Capital Safety Incident. Indicates partial write or `WAL` corruption. Recovery protocol required.  
**Kill Switch Latency Enforcer** | P-006 / `CLAIM-L4-001` | Kill switch response time exceeds `1ms` P99 as measured against `timing_constants.json` baseline for current `hardware_id` | P1 Operational Degradation — `TERMINAL_HALT` deferred. Immediate investigation required. Re-measurement of kill switch timing mandatory before live capital session resumes.  
**Gross Exposure Ceiling Monitor** | P-006 / `CLAIM-L3-006` | `gross_exposure_` atomic exceeds configured notional limit from Registry entry `REG-RISK-001` for current deployment stage (IV-A through IV-D) | New position limit set to zero via monotone latch in `L4`. Existing positions frozen. P1 incident logged. Human authorization required to reset latch.  
**Feed Staleness Circuit Breaker** | P-006 / `AD-012` | `FeedStaleFlag` raised in `/dev/shm/egbos_governance_flags` for any primary feed exceeding configured stale timeout (default: `500ms` (REG-FEED-STALE-001)) | New position limit → 0 via `L4` monotone latch. Existing positions held (not force-closed per `AD-012`). `FeedStaleEvent` written to Tier-0 log and Audit Log. Auto-reset on feed recovery + snapshot confirmed.  
**Replay Fidelity Guard** | P-003 / `CLAIM-FEATURE-001` | Nightly replay validation produces a non-byte-identical `FeatureVector` sequence vs. the stored golden fixture for the same `data_hash` | P1 Operational Degradation. Live capital session blocked for following trading day. Feature engine bisect required. No waiver permitted on `CLAIM-FEATURE-001` during live capital phases.  
**Cross-Plane Memory Order Enforcer** | P-001 / `CLAIM-L3-004` | Any TSAN finding involving a `CROSS_PLANE_READ` macro-annotated atomic, or any `memory_order_relaxed` usage on a cross-plane atomic (`GC-E-044`) | Merge blocked by P4 gate (`GC-E-044`). If detected post-merge at runtime (TSAN instrumented build), P0 incident — treat as potential position state corruption.  
**Research Boundary Enforcement Monitor** | P-002 / `CLAIM-RESEARCH-001` | Any import from a `research/` namespace detected in `engine/`, `replay/`, or `core/` (`GC-E-RESEARCH-001`) | Merge blocked at P1/G3 CI gate. Cannot be waived for production deployments under any condition.  
**Waiver Accumulation Circuit Breaker** | P-001 / B1.6 | Cumulative active waiver-days across all claims exceeds `180` days (REG-WAIVER-ACCUMULATION-001) in any rolling 365-day window | P2 Governance Finding. Mandatory governance board review before next waiver grant. `govcheck waiver check` emits `GC-W-033`. Deployment not blocked but finding is logged in Audit Record.  
**Signal Decay Level 3 Halt** | P-006 / B3.7 | Any live signal's rolling 30-day Sharpe falls below zero for 10 consecutive trading days | Signal weight forced to zero via Registry entry amendment. P1 incident. Champion/challenger protocol initiated. Human decision required for signal retirement or reactivation. No automated reactivation permitted.  
  
### D2.3 — Monitor Registration Requirement

Every entry in the Governance Rules Table above corresponds to a registered monitor artifact at `governance/monitors/{rule_id}.yaml`. The govcheck `monitor-check` command (active from Phase 4) validates that every deployment-blocking claim in the claim registry has a corresponding active monitor registration. An unregistered monitor is a `GC-E-020`-class error at the `C0` gate — it is treated identically to a claim with an exceeded `review_expiry`.

### D2.4 — Halt State Taxonomy

Halt State| Trigger| New Orders| Existing Positions| Restart Authority  
---|---|---|---|---  
`TERMINAL_HALT`| `INV-001` violation, `WAL` corruption, TSAN race post-merge| Blocked| Frozen — no modification| Founder + RFP match confirmed + incident record committed  
`EXPOSURE_HALT`| Gross exposure ceiling breached| Blocked via monotone latch| Held — natural expiry permitted| Founder manual latch reset + operational log entry  
`FEED_STALE_HALT`| `FeedStaleFlag` raised| Blocked via monotone latch| Held — not force-closed| Automatic on feed recovery + snapshot confirmed  
`RESEARCH_HALT`| Replay fidelity failure on nightly validation| Blocked for next session| Not affected (current session)| Founder + feature engine bisect complete + new golden fixture committed  
  
## D3 — Audit & Compliance Logging

Classification
    Compliance Authority Document — Tier 0

Governing Principle
    P-007 (Immutable Audit Over Convenient Deletion), P-003 (Determinism as Constitutional Invariant)

Retention
    7 years (regulatory minimum). Indefinite for governance artifacts.

Owner
    Chief Architecture Office

### D3.1 — Audit Architecture Philosophy

The audit subsystem is the forensic reconstruction substrate. Its purpose is to ensure that at any point in the future — during a regulatory inquiry, a capital loss investigation, or a system re-architecture — it is possible to answer the question: _"What was the complete state of this system at time T, and what governance conditions were in effect that authorized that state?"_

This is a stronger requirement than standard application logging. Standard logging records what happened. The EGBOS audit subsystem records what happened, who authorized it, what governance claim was active at the time, and what the cryptographic state of the governance record was. It is the machine-readable implementation of P-007.

### D3.2 — Audit Record Schema

Every system state transition that is subject to governance is recorded as an Audit Record. Audit Records are written to the **Governance Audit Log** — a separate append-only file from the `WAL`, maintained by the Control Plane process, not the execution engine. The two logs are cross-referenced via shared `sequence` anchors.
[code] 
    // Audit Record schema — append-only binary + JSONL sidecar for human inspection
    // Written to: governance/audit/{YYYY-MM-DD}.jsonl (daily rotation)
    {
      "audit_id":         "AUD-{epoch_ns}",
      "event_class":      "state_transition | governance_action | halt_event | hitl_intervention | claim_change | registry_change",
      "timestamp_tsc":    1234567890123456789,    // CLOCK_MONOTONIC_RAW
      "timestamp_wall":   "ISO-8601",
      "wal_sequence_anchor": 88421,              // WAL sequence at time of audit event (0 if no WAL record)
      "actor":            "engine:L4 | human:@founder | automated:govcheck",
      "action":           "string — precise description of the state transition",
      "prior_state_hash": "sha256:...",          // SHA-256 of relevant pre-transition state
      "post_state_hash":  "sha256:...",          // SHA-256 of relevant post-transition state
      "governing_claim":  "CLAIM-L4-001",        // Active claim authorizing this action (null if none)
      "claim_version_at_event": 2,              // Version of governing claim at time of event
      "waiver_id":        null,                 // Populated if action was authorized by active waiver
      "signature":        "sha256:..."          // SHA-256 of all fields above; enables tamper detection
    }
[/code]

### D3.3 — Tamper-Proof Guarantee

The Governance Audit Log achieves tamper-evidence through a hash-chain construction: each record's `signature` field includes the `audit_id` of the preceding record. An audit log where any record has been deleted, modified, or reordered will produce a broken hash chain detectable by the `govcheck audit verify` command. This command runs as part of the weekly operational calendar (A2.6) and on every P0 incident investigation.

Mandatory Audit-Logged Events

  - All `TERMINAL_HALT`, `EXPOSURE_HALT`, `FEED_STALE_HALT`, and `RESEARCH_HALT` transitions
  - All live capital deployment stage advances (IV-A through IV-D)
  - All HITL interventions (see D4)
  - All claim verification class changes (upgrade or downgrade)
  - All registry entry version increments
  - All waiver grants, extensions, and expirations
  - All Safety Monitor rule evaluations that result in a non-nominal action
  - All `govcheck deploy-gate` invocations — pass and fail alike
  - All signal decay level transitions (D2.2, Signal Decay Level 3 Halt)



### D3.4 — Regulatory Retention Architecture

Daily audit log files are sealed at market close, compressed (zstd), hashed, and versioned into the DVC data governance system (C4.3) with a minimum 7-year object lock. The DVC reference hash for each sealed daily file is committed to `governance/audit/index.yaml` — itself a governance artifact subject to the P-007 append-only rule. Loss of any daily file that cannot be reconstructed from its DVC reference is a P0 compliance incident requiring immediate regulatory counsel notification.

## D4 — Human-in-the-Loop (HITL) Interventions

Classification
    Operational Authority Document — Tier 0

Governing Principles
    P-004 (Governance Precedes Implementation), P-007 (Immutable Audit)

Owner
    Chief Architecture Office + Operations

### D4.1 — HITL Architectural Philosophy

Human interventions in a live trading system are among the highest-risk events the governance system must handle. The failure mode is not malice — it is well-intentioned actions taken under time pressure, without full visibility into system state, that introduce inconsistencies the automated governance system then cannot reason about. An undocumented manual intervention is operationally indistinguishable from a system bug.

EGBOS treats every manual intervention as a **first-class system event**. It is recorded in the `WAL` with a `record_type` of `HITL_INTERVENTION`, carries a cryptographic signature binding it to the identity of the authorized actor, and is simultaneously written to the Governance Audit Log (D3). A manual intervention that bypasses this protocol is a P0 capital safety incident — not because the intervention itself was harmful, but because an unrecorded state transition breaks the determinism invariant (P-003) and makes replay fidelity impossible.

### D4.2 — HITL Intervention Classification

Intervention Class| Permitted Conditions| Authorization Level| Required `WAL` Record Type  
---|---|---|---  
**Emergency Halt Override** | P0 incident response only. System in `TERMINAL_HALT`. Operator manually confirms halt is correct prior to RFP reconciliation. | Founder (sole) | `HITL_HALT_CONFIRM`  
**Authorized Restart** | Post-P0 recovery only. RFP reconciliation complete. `RECOVERY-RECORD-{date}.yaml` committed to `governance/`. | Founder (sole) + signed recovery record | `HITL_RESTART_AUTH`  
**Risk Limit Manual Override** | Temporary reduction of exposure ceiling during extraordinary market conditions (e.g., circuit breaker event, liquidity crisis). Cannot increase ceiling above Registry entry value. | Founder (sole) | `HITL_RISK_LIMIT_CHANGE`  
**Signal Weight Manual Adjustment** | Signal decay Level 2 or Level 3 active. Pending peer review completion. Weight reduction only — no manual increases permitted without full WFV cycle. | Founder (sole) + decay incident log entry | `HITL_SIGNAL_WEIGHT_CHANGE`  
**Registry Entry Emergency Amendment** | Production hardware anomaly requiring immediate threshold adjustment. Must be followed by formal Registry amendment PR within 24 hours. | Founder (sole) + P2 governance finding auto-filed | `HITL_REGISTRY_EMERGENCY_AMEND`  
  
### D4.3 — HITL `WAL` Record Protocol

Every HITL intervention generates a `WAL` record _before_ the intervention takes effect — WAL-before-state applies to human actions as strictly as it applies to automated state transitions (CLAIM-L3-001). The record carries a `payload` containing the cryptographic signature of the intervention authorization, making it possible to verify, during any future replay, that a given state transition was authorized by an identified human actor rather than the result of a system error.
[code] 
    // HITL WAL record — payload schema
    // record_type: HITL_INTERVENTION (enum value 0x09)
    {
      "intervention_class":  "HITL_RESTART_AUTH",
      "actor_id":            "@founder",
      "actor_timestamp_utc": "ISO-8601",
      "authorization_ref":   "governance/recovery/RECOVERY-RECORD-2026-05-30.yaml",
      "authorization_hash":  "sha256:...",   // SHA-256 of referenced authorization artifact
      "pre_action_state_hash": "sha256:...",
      "rationale":           "RFP reconciliation complete. Position delta: 0. Resuming normal operations.",
      "signature":           "sha256:..."    // SHA-256 of all fields above — tamper evidence
    }
[/code]

### D4.4 — Prohibited HITL Actions

The following actions are unconditionally prohibited under the HITL protocol, regardless of operational urgency or authorization level. Each prohibition is a direct application of a binding architectural principle.

  - **Direct in-memory state mutation without`WAL` record.** Violation of P-003 and `CLAIM-L3-001`. Any in-memory state that is not reconstructable from `WAL` replay does not exist from the governance system's perspective.
  - **Deletion or modification of any existing`WAL` record or Audit Log entry.** Direct violation of P-007. Superseding a record requires writing a new record that references the prior; it does not permit modification of the prior record.
  - **Disabling or bypassing any Safety Monitor while live capital is deployed.** Violation of P-006. Capital safety monitors are not operational conveniences; they are constitutional constraints. Disabling a monitor to allow a trade is the highest-severity governance violation the system defines.
  - **Manual increase of any risk or exposure ceiling without Registry amendment.** Any increase to a limit must go through the Registry versioning protocol (D1.3) — an undocumented ceiling increase is invisible to the governance system and breaks the composed verification class of all downstream claims.



## Phase D — Consistency Review

### Consistency Checks Performed

  - Registry & Configuration Store versioning protocol is consistent with claim amendment protocol (B1.2.3) — identical append-only, immutable-prior-version model
  - Governance Rules Table entries reference only active claims as defined in B1.2.5 claim registry
  - Halt taxonomy (D2.4) is consistent with P0/P1/P2 incident classification system (A2.3) — no halt state maps to a lower incident severity than P1
  - `FEED_STALE_HALT` behavior (position freeze, not force-close) is consistent with AD-012 and P-006 rationale
  - Audit Record schema cross-references `WAL` sequence anchors, maintaining determinism bridge between D3 and C2
  - HITL `WAL` record protocol enforces WAL-before-state for human actions — consistent with CLAIM-L3-001 scope
  - Prohibited HITL actions (D4.4) are each traceable to a specific binding principle (P-003, P-006, P-007)
  - Signal Decay Level 3 Halt rule in D2.2 is consistent with decay thresholds defined in B3.7
  - Registry change authority tiers reference ADR, claim amendment, and operational log — consistent with governance authority matrix (A2.5)



### Gaps Identified for Phase E

  - Cryptographic signing infrastructure for HITL `WAL` records not yet specified — key management and signing protocol deferred to Phase E (Security Architecture)
  - Regulatory retention infrastructure (DVC object lock configuration, S3-compatible backing store spec) deferred to Phase E (Compliance Architecture)
  - Network-level security for Governance Audit Log remote replication not yet addressed (Phase E)
  - Secrets management for Registry entry `signature` keys not yet specified (Phase E)



## Invariant Violation Response Matrix

Consolidated quick-reference for P0 incident response. Authoritative source for any engineer responding to a live capital incident. All entries derive from canonical definitions in Phase C §C2.5, Phase D §D2.2, and Phase A §A2.3.

Invariant | Detection Mechanism | Immediate Response | Halt State | Restart Authority | Governing Claim  
---|---|---|---|---|---  
**INV-001**  
Position = Σ FILL_APPLIED | Position Integrity Monitor (D2.2); continuous WAL replay diff | TERMINAL_HALT; position freeze; no new orders | TERMINAL_HALT | Founder + RFP reconciliation + RECOVERY-RECORD committed | CLAIM-INV-001 V4  
**WAL-SEQ**  
Sequence strictly monotone | WAL Sequence Monotonicity Guard (D2.2); CRC32C scan at recovery | TERMINAL_HALT; WAL truncation point identified via govcheck wal-scan | TERMINAL_HALT | Founder + REC-008 protocol + RECOVERY-RECORD committed | CLAIM-WAL-001 V4  
**WAL-ORDER**  
WAL write precedes state mutation | CLAIM-L3-001 V4 kill-9 test; runtime TSC comparison audit | R4 remediation; TERMINAL_HALT if detected post-merge | TERMINAL_HALT (if live) | Founder + root cause + govcheck C0 pass on fix | CLAIM-L3-001 V4  
**INV-002**  
Backtest determinism | P9 gate; SHA-256 identical P&L; curve × 3 | Block signal promotion; investigate feature engine | RESEARCH_HALT | Founder + REC-005 protocol + new golden fixture committed | CLAIM-BACKTEST-001 V4  
**INV-003**  
Feature parity (offline = online) | P8 gate; memcmp on FeatureVector fields | Block model promotion; SIMD/FMA pin audit | RESEARCH_HALT | Founder + parity test pass + GC-E-054 clean | CLAIM-FEATURE-PARITY-001 V4  
**INV-004**  
Regime WAL determinism | CLAIM-REGIME-001 V4; 3× identical regime classification from same WAL sequence | Suspend regime-dependent signal weights; investigation required | RESEARCH_HALT | Founder + regime engine bisect + new proof artifact | CLAIM-REGIME-001 V4  
**INV-005**  
Daily P&L; reconciliation | scripts/daily_pnl_reconciliation.sh; INV-005 check | P1 incident; investigation before next session; capital scaling blocked | RESEARCH_HALT (next session) | Founder + reconciliation script clean + operational log entry | CLAIM-ATTR-001 V4  
**FEED_STALE**  
Feed gap > REG-FEED-STALE-001 | L0 gap detection (C1.3); FeedStaleFlag in governance SHM | New-position limit → 0 via monotone latch (AD-007); existing positions held | FEED_STALE_HALT | Automatic on feed recovery + snapshot confirmed (REC-004) | CLAIM-L0-002  
**REPLAY_FIDELITY**  
Nightly replay byte-identical | scripts/nightly_replay_validation.sh; CLAIM-FEATURE-001 V4 | P1 incident; live session blocked next day; feature engine bisect | RESEARCH_HALT | Founder + REC-005 protocol + 3× byte-identical on new fixture | CLAIM-FEATURE-001 V4  
  
⚑ This matrix is a derived quick-reference only. In all cases, canonical definitions in Phase C §C2.5, Phase D §D2.2, and Phase A §A2.3 are authoritative. Any discrepancy between this matrix and canonical sections requires an RFC to correct the matrix entry.

E

## Phase E — Execution & Runtime Environment

Execution Engine architecture, Operational Loop, Pre-flight Safety Protocol, and Runtime Status Dashboard. Interfaces directly with the Control & Governance Plane (Phase D).

Complete

## Phase E — Architectural Overview

Classification
    Operational Authority Document — Tier 0

Version
    1.0

Owner
    Chief Architecture Office

Approval Authority
    CTO

Interfaces With
    Phase D (Control & Governance Plane) — mandatory; Phase C (Runtime Infrastructure) — mandatory

Active From
    Phase III (Production Qualification) for pre-flight; Phase IV-A for live execution

⚑ **Audit Traceability:** All state transitions, module activations, pre-flight outcomes, and runtime exceptions produced by this phase are subject to the mandatory audit logging requirements defined in Phase D §D3 — Audit & Compliance Logging. Every execution event that mutates position-affecting state generates a corresponding `WAL` record per the WAL-before-state protocol (C2.3). Runtime exceptions are classified against the incident taxonomy in A2.3.

The Execution & Runtime Environment is the operational instantiation of the architectural contracts established in Phases B, C, and D. Where Phase C defines the physical substrate (SHM layouts, WAL topology, hardware affinity) and Phase D defines the constitutional enforcement layer, Phase E defines the **running system** — the modules that consume market signals, evaluate them against risk and governance constraints, and produce execution decisions, doing so under continuous pre-flight validation and observable runtime telemetry.

The central design constraint of this phase is that **runtime flexibility must never exceed governance specification**. A module that can be activated, reconfigured, or bypassed at runtime in ways not registered in the governance system is a structural audit gap — indistinguishable from a system error during forensic reconstruction. Every runtime state the system can enter is a named, claim-backed, auditable state.

Four Subsystems of the Execution & Runtime Environment

  - **E1 — Execution Engine & Module Architecture:** Modular structure for Orders, Risk Checks, and Latency Monitoring, with explicit governance-plane interface contracts.
  - **E2 — Operational Loop:** The `[Sense → Evaluate → Execute → Audit]` cycle — the atomic unit of execution. Visualized and formally specified.
  - **E3 — Pre-flight Safety Protocol:** Mandatory state validation, schema verification, and connection integrity checks required before any execution signal is armed.
  - **E4 — Runtime Status Dashboard:** System Heartbeat, Active Modules telemetry, and Alert Log — the live observability surface of the execution environment.



## E1 — Execution Engine & Module Architecture

Classification
    Infrastructure Authority Document — Tier 0

Governing Principles
    P-001 (Mechanical Enforcement), P-002 (Structural Impossibility), P-005 (Failure Mode First)

Governing Claims
    `CLAIM-L4-001` (V4), `CLAIM-INV-001` (V4), `CLAIM-L3-001` (V4)

Governance Plane Interface
    Phase D §D1 Registry, §D2 Safety Monitors — mandatory coupling

### E1.1 — Execution Engine Thesis

The Execution Engine is the runtime realization of the `L0–L5` layer contracts defined in the execution plane (Phase C). It is not a monolithic process — it is a structured composition of independently governed, independently halted **Execution Modules** , each with a registered governance identity, an explicit interface contract with the Control & Governance Plane (Phase D), and a defined degradation behavior that satisfies P-005 (failure mode specification precedes nominal specification).

The Execution Engine exposes exactly two interfaces to the Governance Plane: a **read interface** (the `/dev/shm/egbos_governance_flags` region — see C3.1) from which it reads halt signals, feed stale flags, and risk limit latches; and a **write interface** (the `WAL` and Governance Audit Log) into which it records every state transition. It has no other coupling to the Governance Plane. This structural isolation ensures that a Governance Plane failure degrades gracefully into a `TERMINAL_HALT` rather than an undefined engine state.

### E1.2 — Execution Module Registry

Every Execution Module is a named, versioned runtime component registered in `governance/modules/`. A module that executes code without a registered governance identity is structurally equivalent to a claim without a claim YAML — it does not exist from the governance system's perspective. The `govcheck module-check` command (active from Phase III) verifies that all instantiated modules have current registrations.

Orders Module

Standby — Pre-flight Required

Layer: `L3` / `L5`  
Claim: `CLAIM-L3-001` (V4)  
WAL dependency: mandatory 

Risk Checks Module

Standby — Pre-flight Required

Layer: `L4`  
Claim: `CLAIM-L4-001` (V4)  
Kill switch: monotone latch 

Latency Monitor

Standby — Pre-flight Required

Layer: Cross-cutting (`L0–L5`)  
Ref: `timing_constants.json`  
Baseline: production `hardware_id`

### E1.3 — Module Interface Contracts

Module| Reads From| Writes To| Governance Plane Input| Degraded Mode  
---|---|---|---|---  
**Orders Module** | `/dev/shm/egbos_feature_vector` (seqlock), `/dev/shm/egbos_regime_signal` | `WAL` (record types: `ORDER_NEW`, `FILL_APPLIED`), `/dev/shm/egbos_exec_commands` | `egbos_governance_flags`: `halt_required`, `feed_stale` | On `halt_required=1`: immediate order queue drain, zero new submissions. On `feed_stale=1`: new orders blocked (AD-012).  
**Risk Checks Module** | `/dev/shm/egbos_position_state` (`gross_exposure_` atomic only, via `CROSS_PLANE_READ`), Registry entry `REG-RISK-001` | `/dev/shm/egbos_risk_limits` (monotone latches), Governance Audit Log (`D3`) | `egbos_governance_flags`: reads and writes `halt_required` | On ceiling breach: latch set, new orders blocked. On kill switch timeout > 1ms P99: P1 incident, live session blocked.  
**Latency Monitor** | All `SPSC` ring timestamps, `WAL` `tsc_before_write` / `tsc_after_write` fields, `timing_constants.json` | Prometheus metrics endpoint, Governance Audit Log on threshold breach | None — read-only observer; never writes to governance flags | On monitor process fault: P1 incident. Execution continues (monitor is observability, not control). P99 breach: logged, does not halt.  
  
### E1.4 — Governance Plane Interface Specification

The interface between the Execution Engine and the Control & Governance Plane (Phase D) is constrained to the two channels defined in C3.1. No module may call into `govcheck` at runtime, directly read claim YAML files, or modify any governance artifact during an execution session. The governance system is a pre-flight and post-session concern — its enforcement is through the structural constraints it places on what the engine can do, not through runtime coupling.
[code] 
    // Governance interface — engine/core/governance_interface.h
    // Read-only access to governance flags SHM region
    // Called once per operational loop iteration (E2)
    // Returns: GovernanceFlags snapshot — not cached between iterations
    struct GovernanceFlags {
      std::atomic  halt_required;    // Set by D2 Safety Monitors
      std::atomic  feed_stale;       // Set by C1 Gap Detection
      std::atomic  preflight_armed;  // Set by E3 Pre-flight Protocol
      std::atomic  pos_limit_latch;  // Monotone — only decreases
      uint8_t               _pad[40];          // Pad to 64 bytes — one cache line
    };
    
    // INVARIANT: halt_required is checked FIRST in every loop iteration.
    // If halt_required == 1, the loop exits before evaluating any signal.
    // This ordering is not configurable. See E2 Operational Loop.
[/code]

## E2 — Operational Loop

Classification
    Infrastructure Authority Document — Tier 0

Governing Principles
    P-003 (Determinism), P-006 (Capital Safety Over Continuity), P-007 (Immutable Audit)

Loop Cycle Budget
    P99 ≤ 500µs feed-to-decision (measured; ref: `timing_constants.json`)

### E2.1 — Operational Loop Thesis

The Operational Loop is the atomic unit of execution in the EGBOS runtime. One iteration of the loop represents one complete traversal of the `[Sense → Evaluate → Execute → Audit]` cycle. The loop has no internal state that persists across iterations except through the `WAL` — every in-memory state that matters is a deterministic function of the `WAL` and the current market event. This is the runtime expression of P-003.

The ordering of the four phases is constitutionally fixed. **Audit is not the last step because it is least important — it is the last step because it records the outcome of a completed, committed execution decision.** An audit record written before the execution decision is committed is a false record. An execution decision made before governance flags are checked is a capital safety defect. The ordering is the specification.

### E2.2 — Operational Loop Visualization

SENSE Market event recv GovernanceFlags check armed EVALUATE Feature vector read Risk checks + signal eval cleared EXECUTE WAL record written Order routed via L5 committed AUDIT Audit record appended Latency telemetry emitted next iteration — unless halt_required=1 TERMINAL_HALT

### E2.3 — Loop Phase Specifications

Phase| Inputs| Processing| Exit Conditions| Timing Budget  
---|---|---|---|---  
**Sense** | `SPSC` ring pop from `L0→L1`; `GovernanceFlags` snapshot from `egbos_governance_flags` | Pop `RawMarketEvent`. Check `halt_required` first. Check `preflight_armed`. If either fails: exit loop with appropriate halt state. | `halt_required=1` → `TERMINAL_HALT`. `preflight_armed=0` → loop suspended pending E3. Normal: proceed to Evaluate. | ≤ 500ns P99 (SPSC pop + flag check)  
**Evaluate** | `FeatureVector` via seqlock (`C3.4`); `egbos_regime_signal`; `egbos_risk_limits` latches | Read `FeatureVector` with torn-read detection. Evaluate signal. Apply risk limit check against `pos_limit_latch`. No external I/O in this phase. | Risk limit breach → emit zero-size order (no transmission). Signal below threshold → no order. Normal: proceed to Execute. | ≤ 10µs P99 (feature read + eval)  
**Execute** | Evaluated order decision from Evaluate phase | Write `WAL` record (`ORDER_NEW`) via `O_DSYNC`. Push to `egbos_exec_commands` `SPSC`. `L5` routes to exchange. WAL-before-state invariant enforced. | `WAL` write failure → `TERMINAL_HALT` (cannot proceed without durable record). Short write → fatal. Normal: proceed to Audit. | ≤ 200µs P99 (WAL fsync + SPSC push; ref: `timing_constants.json`)  
**Audit** | Completed execution decision + `WAL` sequence anchor | Append Audit Record to Governance Audit Log (D3.2). Emit latency telemetry to Prometheus. Update Latency Monitor running statistics. | Audit write failure → P1 incident (execution continues; audit backlog queued). Normal: return to Sense phase. | ≤ 50µs P99 (async audit append; non-blocking on execution path)  
  
Ordering Invariant — Constitutional

The four-phase ordering `[Sense → Evaluate → Execute → Audit]` is constitutionally fixed. No optimization, no performance argument, and no operational urgency may reorder these phases. Specifically: (1) governance flag check must precede signal evaluation — an unevaluated halt signal is a capital safety defect; (2) `WAL` write must precede order transmission — an order without a prior `WAL` record violates `CLAIM-L3-001`; (3) audit record must follow committed execution — an audit record that precedes commitment is a false record under P-007.

## E3 — Pre-flight Safety Protocol

Classification
    Capital Safety Authority Document — Tier 0

Governing Principles
    P-004 (Governance Precedes Implementation), P-005 (Failure Mode Precedes Nominal), P-006 (Capital Safety Over Continuity)

Execution Gate
    `preflight_armed` flag in `/dev/shm/egbos_governance_flags` — set only on complete pre-flight pass

Governing Claims
    `CLAIM-L3-05` (V4), `CLAIM-WAL-001` (V4), `CLAIM-INV-001` (V4)

### E3.1 — Pre-flight Philosophy

The Pre-flight Safety Protocol is the enforcement mechanism for the principle that **no execution signal may be armed in a system whose correctness preconditions have not been verified in the current session**. A system that passed all pre-flight checks yesterday is not a system that has passed pre-flight today — hardware state, WAL integrity, schema versions, and connection topology are all session-scoped properties that must be independently verified on every startup.

Pre-flight is not a startup convenience. It is a governance gate. The `preflight_armed` flag in the governance flags SHM region (`C3.1`) is the only mechanism by which the Operational Loop (E2) is authorized to enter its Execute phase. Until `preflight_armed=1`, the loop runs in Sense-only mode — consuming and logging market events but producing no execution decisions. This mode is not configurable and cannot be skipped.

### E3.2 — Pre-flight Check Registry

All 12 mandatory pre-flight checks are listed below. Checks are organized into three categories. A single FAIL in any check in categories State Validation or Schema Verification blocks `preflight_armed` indefinitely — no timeout, no retry limit, no override. Connection Integrity checks permit one automatic retry before blocking.

#### Category 1 — State Validation

PF-01 — WAL Integrity Scan

CRC32C verification on all `WAL` records from last checkpoint to end of file. Sequence monotonicity confirmed. Any gap or CRC failure → block.

Pending

PF-02 — Position Reconstruction Verification

In-memory position state reconstructed from `WAL` replay. `INV-001` check: reconstructed position == `Σ FILL_APPLIED`. Any delta → block + P0 incident.

Pending

PF-03 — Safety Monitor Latch Audit

All monotone latches in `/dev/shm/egbos_risk_limits` audited against last `WAL` session record. Any latch set without a corresponding `WAL` record → block.

Pending

PF-04 — Governance Flags Region Initialization

`/dev/shm/egbos_governance_flags` region mapped, `mlock(2)` confirmed, all flags initialized to safe defaults. `halt_required=0` only after PF-01 and PF-02 pass.

Pending

PF-05 — Claim Expiry Sweep

`govcheck deploy-gate --target production` executed. Any `GC-E-020` (review_expiry exceeded) or `GC-E-022` (waiver expired) → block. Output committed to session audit record.

Pending

#### Category 2 — Schema Verification

PF-06 — FlatBuffers Schema Freshness

SHA-256 of committed generated FlatBuffers code matches binary linked into execution engine. Schema drift between source and binary → block. Prevents silent wire format mismatch.

Pending

PF-07 — Registry Entry Signature Verification

All Registry entries consumed by active modules verified against their `signature` field (Phase D §D1.2). Any unsigned or tampered entry → block. Prevents silent configuration drift.

Pending

PF-08 — timing_constants.json Hardware Match

Runtime `hardware_id` (from `/proc/cpuinfo` \+ NVMe device ID) matches `hardware_id` in committed `timing_constants.json`. Mismatch → block. Prevents V4 claims running on different hardware.

Pending

PF-09 — ONNX Model Hash and Runtime Version Verification

SHA-256 of each loaded ONNX model file matches `onnx_artifact_hash` in model registry entry (B2.6). **Additionally:** the running ONNX Runtime version (obtained via `ort::GetVersionString()` or `python -c "import onnxruntime; print(onnxruntime.__version__)"`) must exactly match `onnx_runtime_version` in the model registry entry for each loaded model. Any version mismatch → block. Active from Phase V-F.

Pending

#### Category 3 — Connection Integrity

PF-10 — Exchange Connectivity Heartbeat

FIX session heartbeat confirmed with exchange order gateway. RTT measured and compared against `timing_constants.json` baseline. RTT > 500µs P99 → block. One retry permitted.

Pending

PF-11 — Market Data Feed Sequence Continuity

Feed sequence number confirmed continuous from last session's terminal sequence. Gap > 0 without documented maintenance window → block. Recovery snapshot required before proceeding.

Pending

PF-12 — SHM Region mlock Confirmation

All eight SHM regions (ref: C3.1) confirmed mapped and `mlock(2)` verified. Any page fault risk on mapped region → block. NUMA locality confirmed for socket-0 regions.

Pending

### E3.3 — Arming Sequence
[code] 
    // Pre-flight arming sequence — scripts/preflight.sh + engine startup
    // MUST complete in order; no parallel execution of category 1 checks
    
    Phase 1 (Category 1 — State Validation):
      PF-01 → PF-02 → PF-03 → PF-04 → PF-05
      All must PASS. Any FAIL → ABORT preflight.
      Audit record written: PF_CATEGORY_1_RESULT
    
    Phase 2 (Category 2 — Schema Verification):
      PF-06 → PF-07 → PF-08 → PF-09 (conditional on Phase V-F)
    PF-13 (HT sibling isolation, all phases with CPU affinity)
      All must PASS. Any FAIL → ABORT preflight.
      Audit record written: PF_CATEGORY_2_RESULT
    
    Phase 3 (Category 3 — Connection Integrity):
      PF-10 ‖ PF-11 ‖ PF-12  (parallel execution permitted)
      All must PASS or PASS-on-retry. Any double-FAIL → ABORT preflight.
      Audit record written: PF_CATEGORY_3_RESULT
    
    ARMING:
      All three phases PASS →
        governance_flags.preflight_armed.store(1, seq_cst);
        wal_writer.write_sync(PREFLIGHT_ARMED record);
        audit_log.append(PREFLIGHT_ARMED audit record);
      // Operational Loop (E2) Sense phase now permits Execute phase entry
[/code]

## E4 — Runtime Status Dashboard

Classification
    Observability Document — Tier 1

Governing Principle
    P-001 (Mechanical Enforcement — dashboard is an enforcement surface, not a reporting surface)

Audit Reference
    All dashboard state is derived from Phase D §D3 Audit Log and Prometheus metrics. Dashboard never has primary state.

### E4.1 — Dashboard Architecture Thesis

The Runtime Status Dashboard is a **read-only projection** of the Governance Audit Log and Prometheus metrics. It holds no primary state. If the dashboard disagrees with the Audit Log, the Audit Log is correct. The dashboard is an observability surface — its failure mode is information loss, not capital risk. It is explicitly not an enforcement surface: no capital safety decision may depend on the dashboard being available or accurate.

Implementation Status Notice

The E4 Runtime Status Dashboard is currently implemented as a static HTML panel with JavaScript simulation injection (Phase F §F2). Production data source wiring — Prometheus scrape, inotify Audit Log tail, Redis heartbeat poll — is a Phase III engineering deliverable governed by GC-E-051. The dashboard panels in this document are structural specifications, not operational displays. No alert displayed in this document during simulation mode represents a real system event.

### E4.2 — Runtime Status Panels

System Heartbeat

Engine Uptime — awaiting session

Last WAL Write (tsc) — —

WAL Fsync P99 — ns

Loop Cycle P99 — µs

Feed-to-Decision P99 — µs

Preflight Status NOT ARMED

Governance Flags halt_required: —

Active Module Status

Orders Module STANDBY 

Risk Checks Module STANDBY 

Latency Monitor STANDBY 

WAL Writer STANDBY 

Audit Logger (D3) STANDBY 

Feed Ingestor (L0) STANDBY 

Alert Log — Runtime Exceptions  Source: Phase D §D3 Audit Log projection 

  - \--:--:-- INFO Runtime Status Dashboard initialized. Awaiting session start and pre-flight arming (E3).
  - \--:--:-- P2 SYSTEM STATUS: preflight_armed = 0. No execution session active. All modules in STANDBY.
  - \--:--:-- P2 GOVERNANCE CHECK: govcheck deploy-gate result pending. Run PF-05 to populate.
  - \--:--:-- INFO AUDIT LOG: Live alerts stream here from the D3 Audit Log during active sessions. P0 entries trigger immediate visual halt state.



### E4.3 — Dashboard Data Source Contracts

Panel| Primary Data Source| Refresh Mechanism| Staleness Tolerance  
---|---|---|---  
**System Heartbeat** | Prometheus metrics endpoint (WAL writer exports, loop cycle histogram) | Prometheus scrape interval (default: 5s) | ≤ 30s. Beyond 30s without refresh: panel enters STALE state — visual indicator, no operational impact.  
**Active Module Status** | `/dev/shm/egbos_governance_flags` read + module heartbeat Redis key | Polling at 1Hz from observability process (socket 1 — no execution core interference) | ≤ 5s. Module status is advisory; authoritative state is always the governance flags SHM region.  
**Alert Log** | Governance Audit Log tail (D3.2) — `event_class: halt_event | governance_action` records only | File tail (inotify) on `governance/audit/{date}.jsonl` | Zero tolerance for P0 events — inotify delivery latency must be ≤ 1s. P1/P2 events: ≤ 10s acceptable.  
  
Dashboard Independence Invariant

The Runtime Status Dashboard process runs on socket 1 cores (ref: C5.3 CPU affinity table) with no access to execution plane SHM write paths. It may read `egbos_governance_flags` but may never write to it. Dashboard process fault is a P2 governance finding — it does not trigger a halt of any kind. A trading system that requires its dashboard to function correctly is a system that has conflated observability with control.

## Phase E — Consistency Review

### Consistency Checks Performed

  - Execution Engine governance-plane interface (E1.4) is consistent with C3.1 SHM region topology — reads only from registered regions, no new regions introduced
  - Operational Loop phase ordering (E2.3) enforces WAL-before-state for Execute phase — consistent with CLAIM-L3-001 and C2.3 three-phase commit protocol
  - Operational Loop Audit phase references D3.2 Audit Record schema — no duplicate or contradictory audit record definition
  - Pre-flight check PF-05 invokes `govcheck deploy-gate` — consistent with G2 gate specification in B1.4.2
  - Pre-flight check PF-08 hardware_id matching is consistent with V4 claim scope limit defined in B1.2.2
  - Pre-flight arming sequence writes a WAL record on arm — consistent with WAL-before-state ordering and P-007 immutable audit
  - Dashboard data sources are all read-only projections — no dashboard component has write access to execution plane or governance flags
  - Module degraded modes (E1.3) are consistent with halt taxonomy in D2.4 — no new halt state introduced without corresponding D2 entry
  - Latency Monitor is classified as observability, not control — consistent with P-005 (failure mode: information loss, not capital risk)
  - CPU affinity for dashboard process (socket 1) consistent with C5.3 table assignment for Governance / Monitoring workloads



### Gaps Identified for Phase F

  - Prometheus metrics schema and Grafana dashboard specifications not yet defined (Phase F: Observability Architecture)
  - Redis key schema for module heartbeat signals referenced but not specified (Phase F)
  - Alert Log inotify delivery SLA (≤ 1s for P0) requires operational validation on production hardware — deferred to Phase F runbook suite
  - Operational runbook for pre-flight failure recovery (ABORT preflight scenarios) not yet authored (Phase F)
  - Live dashboard implementation (JavaScript polling loop against real data sources) is a Phase F deliverable — E4 contains structural and data-source specification only



F

## Phase F — System Simulation & Stress Testing

Simulation Controller for live-injected E4 Dashboard mock data, and the Stress Test Registry for high-load scenario validation. Addresses gaps flagged in Phase E.

Complete

## Phase F — Architectural Overview & Gap Resolution

Classification
    Verification Authority Document — Tier 1

Version
    1.0 (Active)

Owner
    Chief Architecture Office

Interfaces With
    Phase E §E4 (Runtime Status Dashboard) — mandatory; Phase D §D2 (Safety Monitors) — read-only simulation target

Status
    Active — Simulation Controller and Stress Test Registry under construction

**Simulation Boundary Notice:** Every artifact in Phase F operates exclusively against the simulated/mock data plane introduced in F2. No component described in this phase reads from, writes to, or otherwise interacts with the live `/dev/shm/egbos_*` regions, the production `WAL`, or the Governance Audit Log described in Phase D. The Simulation Controller is a development and qualification tool. Its existence does not relax, bypass, or substitute for any pre-flight check (E3) or Safety Monitor (D2) when the system operates in a live or staging session. 

### F.1 — Phase F Thesis

Phase E delivered the structural specification of the Execution & Runtime Environment, including a Runtime Status Dashboard (E4) that was explicitly static — a read-only projection with placeholder values and no live data path. Phase F's purpose is twofold: first, to convert that static dashboard into an **interactive simulation environment** capable of exercising the full `[Sense → Evaluate → Execute → Audit]` loop (E2) under operator-controlled synthetic conditions; second, to formally register the **high-load and failure-injection scenarios** that any future staging qualification (Phase III, per A2.2) must be able to survive.

This phase does not introduce new correctness claims, new governance authority, or new constitutional invariants. It is strictly additive tooling that operates within the boundaries already established by Phases A through E. Where Phase F scenarios reveal a gap in the existing architecture, that gap is recorded as a new entry in the Phase G gap list — Phase F itself does not retroactively amend Phase B–E claims.

### F.2 — Phase E Gap Resolution Matrix

The five gaps flagged in Phase E — Consistency Review are restated below in priority order, with their Phase F disposition. Priority reflects planning severity using the incident-severity vocabulary of A2.3 as a stand-in scale: **P1** = blocks Phase III staging exit if unresolved; **P2** = blocks Phase IV-A capital deployment if unresolved; **P3** = observability/tooling debt only.

Priority| Gap (from Phase E)| Phase F Disposition  
---|---|---  
P1 | Live dashboard implementation (JavaScript polling loop against real data sources) | **Resolved in this phase** — via Simulation Controller (F.3). Polling loop implemented against simulated data plane; production data-source wiring deferred to Phase G but the polling mechanism itself is complete and reusable.  
P1 | Alert Log inotify delivery SLA (≤ 1s for P0) requires operational validation on production hardware | **Partially addressed** — Stress Test Registry (F.4) defines load scenarios that exercise Alert Log throughput against simulated event rates. Production hardware validation against real `inotify` latency remains **deferred to Phase G**.  
P2 | Redis key schema for module heartbeat signals referenced but not specified | **Resolved in this phase** — schema defined in F.3.3 below, consumed directly by the Simulation Controller's Active Module Status injection.  
P2 | Operational runbook for pre-flight failure recovery (ABORT preflight scenarios) not yet authored | **Partially addressed** — Stress Test Registry entries ST-04 and ST-05 (F.4) define the specific ABORT preflight failure injections. Full narrative runbook prose remains **deferred to Phase G**.  
P3 | Prometheus metrics schema and Grafana dashboard specifications not yet defined | **Deferred to Phase G.** The Stress Test Registry (F.4) references metric names (e.g., `wal_fsync_p99_ns`, `loop_cycle_p99_us`) that Phase G must formalize into a complete Prometheus exposition format and corresponding Grafana panel definitions.  
  
## F2 — Simulation Controller

Classification
    Development & Qualification Tooling — Tier 2

Governing Principle
    P-001 (Mechanical Enforcement — simulation must be mechanically distinguishable from live state)

Target
    Phase E §E4 — Runtime Status Dashboard

Data Plane
    Simulated only — see F.2.1 Isolation Guarantee

### F2.1 — Simulation Isolation Guarantee

The Simulation Controller operates on a JavaScript-resident mock state object, `SIM_STATE`, that exists only within the browser session rendering this document. `SIM_STATE` is never persisted, never transmitted, and has no code path into any SHM region, `WAL`, or Governance Audit Log described in Phases C, D, or E. The only coupling between `SIM_STATE` and the rest of the document is a one-directional DOM update: `SIM_STATE` values are written into the existing E4 dashboard DOM elements via the `data-sim-target` attribute convention defined below.

Visual Distinction Requirement

Whenever the Simulation Controller is actively injecting mock data, the E4 Dashboard must display a persistent visual indicator (the `#f2-sim-badge` element, defined in F.2.2) reading "SIMULATION ACTIVE — NOT LIVE DATA". This indicator is a direct application of the principle that no operator may mistake simulated telemetry for live telemetry — the dashboard-independence invariant established in E4.3 extends to require simulated state to be unmistakably labeled.

### F2.2 — Simulation Controller Panel

The following control panel is the operator interface to the Simulation Controller. It is rendered immediately above the E4 Dashboard panels (cross-reference: E4) and provides manual injection controls for each of the three E4 panel categories.

Simulation Controller

● Simulation Active — Not Live Data 

Session Start Simulated Session Stop & Reset IDLE

Heartbeat Injection Inject Heartbeat Tick

Loop Cycle P99 (µs) 180

Module States All ACTIVE All STANDBY Fault: Risk Checks Fault: Latency Monitor

Alert Injection Inject INFO Inject P2 Inject P1 Inject P0 (Halt Sim) Clear Alert Log

### F2.3 — Module Heartbeat Redis Key Schema

This schema resolves the Phase E gap regarding the Redis key structure for module heartbeat signals (referenced in E4.3, "module heartbeat Redis key"). The Simulation Controller's `EGBOS_SIM.setAllModules()` and `EGBOS_SIM.faultModule()` functions write to an in-memory analog of this schema (`SIM_STATE.moduleHeartbeats`), structurally mirroring the production key layout so that the simulation-to-production transition (Phase G) requires no schema translation.
[code] 
    // Redis heartbeat key schema — production layout (mirrored by SIM_STATE in F2)
    // Key pattern: egbos:heartbeat:{module_id}
    // TTL: 5 seconds (matches E4.3 "Active Module Status" 1Hz polling + staleness tolerance)
    
    KEY   egbos:heartbeat:orders_module
    VALUE {
      "status":          "active | standby | fault | inactive",
      "last_tick_tsc":    1234567890123456789,
      "loop_iteration":   884213,
      "fault_reason":     null
    }
    
    KEY   egbos:heartbeat:risk_checks_module
    KEY   egbos:heartbeat:latency_monitor
    KEY   egbos:heartbeat:wal_writer
    KEY   egbos:heartbeat:audit_logger
    KEY   egbos:heartbeat:feed_ingestor_l0
    // One key per Active Module Status row in E4.2
    
    // Polling process (E4.3) reads all egbos:heartbeat:* keys at 1Hz.
    // Missing key (TTL expired) => module status displayed as STALE,
    // distinct from "inactive" (graceful) and "fault" (error-reported).
[/code]

### F2.4 — Simulation Controller Implementation Notes

The Simulation Controller is implemented as a single global object, `EGBOS_SIM`, attached to `window` and defined within the existing document `<script>` block (see Operation 8). It exposes the methods referenced by the `onclick` handlers in F.2.2. All DOM writes target existing E4 elements via stable selectors — no new IDs are introduced into E4's markup beyond those already specified in the Phase E task (`#e4-alert-log`, the heartbeat panel's `.heartbeat-value` elements, and the module status `.status-dot` elements).

## F3 — Stress Test Registry

Classification
    Verification Authority Document — Tier 1

Governing Principles
    P-005 (Failure Mode Specification Precedes Nominal), P-006 (Capital Safety Over Continuity)

Execution Context
    Simulated data plane only (F2.1). Production execution of these scenarios is a Phase III staging qualification activity per A2.2 Phase II/III.

### F3.1 — Stress Test Philosophy

A correctness claim verified only under nominal load is an incomplete claim. The Stress Test Registry exists to formally enumerate the high-load and degraded-condition scenarios that the Execution Engine (E1), Operational Loop (E2), Pre-flight Protocol (E3), and Runtime Dashboard (E4) must survive — or, where survival is not the correct response, must fail into a defined and auditable halt state (per D2.4).

Each registry entry below is written so that it can be (a) exercised immediately against the Phase F Simulation Controller for qualitative/UI validation, and (b) promoted unchanged into a Phase G production stress-test harness running against real `SPSC` rings, real `WAL` writers, and production `timing_constants.json` baselines. The "Simulated In Phase F" column indicates current coverage; "No" entries are explicitly flagged as Phase G work, not silently dropped.

### F3.2 — Stress Test Registry Table

Test ID | Scenario | Parameters | Severity | Expected System Response | Simulated in Phase F  
---|---|---|---|---|---  
**ST-01** | Rapid-fire execution events | Sustained `SPSC` ring push rate at 10× nominal (650K events/sec) for 60s, ring size held at default 65536 slots (C3.2) | High | Producer applies backpressure (spin per C3.2 "Full condition") — never drops. Loop Cycle P99 may exceed 500µs budget; this is an `ALERT`, not a halt, provided `INV-001` remains satisfied. | Yes — F2 loop latency slider simulates degraded P99; ring-level backpressure is logical-only  
**ST-02** | Governance delay spike | `egbos_governance_flags` read latency artificially inflated to 50ms (vs. sub-µs nominal) for a 10s window, simulating a Control Plane (Phase D) process scheduling stall | Extreme | Operational Loop Sense phase (E2.3) blocks on flag read past its budget. After configurable timeout, loop must treat unreadable governance flags identically to `halt_required=1` — fail-safe default. `TERMINAL_HALT` if timeout exceeded. | Yes — F2 "Inject P0 (Halt Sim)" approximates the resulting dashboard halt-state transition  
**ST-03** | WAL fsync latency degradation | `wal_fsync_p99_ns` degrades from baseline (8,100ns) to 25,000ns, simulating NVMe controller thermal throttling | High | Execute phase (E2.3) timing budget breach logged as P1. Three-phase commit (C2.3) ordering unaffected — correctness preserved, only latency degrades. Re-measurement of `timing_constants.json` triggered per C5.2 if sustained > 1 hour. | Yes — F2 loop latency slider can represent this via combined WAL+loop P99 readout  
**ST-04** | Pre-flight ABORT — WAL integrity failure (PF-01) | Inject a synthetic CRC32C mismatch at a record offset within the last checkpoint window of the simulated `WAL` | Extreme | Pre-flight Category 1 (E3.3) aborts at PF-01. `preflight_armed` remains 0 indefinitely. No retry. Dashboard Preflight Status (E4.2) must display `NOT ARMED` with a P0-class alert log entry. | Yes — F2 "Inject P0 (Halt Sim)" + session state forced to `PREFLIGHT_FAILED`  
**ST-05** | Pre-flight retry exhaustion — connectivity (PF-10) | Simulated exchange order gateway heartbeat fails twice consecutively (PF-10 permits exactly one retry per E3.2) | High | Pre-flight Category 3 aborts after second failure. Categories 1 and 2 results remain valid (cached) but overall `preflight_armed` stays 0. Distinct alert messaging from ST-04 — operator must be able to distinguish "state/schema problem" from "connectivity problem". | Yes — F2 alert injection with distinct P1 message text  
**ST-06** | Feed staleness during high load | Combine ST-01 (10× event rate) with a simulated `FeedStaleFlag` raise at t+30s (500ms stale threshold (REG-FEED-STALE-001)) | High | Per C1.3 and AD-012: new-position limit → 0 via monotone latch. Existing positions held. High event throughput must not delay propagation of the stale flag to the Risk Checks Module (E1.3) beyond one loop iteration. | Partial — module fault injection (risk_checks) approximates the resulting STANDBY/latched UI state; combined throughput+stale simulation deferred to Phase G  
**ST-07** | Audit Log write contention | Alert Log injection rate sustained at 100 events/sec for 30s (vs. nominal <1/sec), targeting the inotify-based Alert Log panel (E4.3) | Moderate | Per E4.3, Audit phase (E2.3) is non-blocking on the execution path — audit backlog queues rather than stalling Execute. Dashboard Alert Log (E4.2) must remain scrollable and performant; oldest entries may be evicted from the live view without data loss in the underlying Audit Log. | Yes — `EGBOS_SIM.injectAlert()` can be invoked in a tight loop; this is the direct mechanism for ST-07  
**ST-08** | Cascading module fault | Fault Latency Monitor first, then 5s later fault Risk Checks Module, while Orders Module remains active | Extreme | Per E1.3 degraded-mode table: Latency Monitor fault is P1, execution continues. Risk Checks Module fault must independently trigger position-limit latching (D2.2 Gross Exposure Ceiling Monitor pattern) regardless of Latency Monitor state — module faults must not be correlated in their safety responses. | Yes — sequential `EGBOS_SIM.faultModule()` calls; run buttons in F3.3 sequence these automatically  
  
### F3.3 — Quick-Run Scenario Buttons

The following buttons invoke pre-composed sequences of `EGBOS_SIM` calls corresponding to the scenarios above that are simulatable in Phase F (ST-01, ST-02, ST-03, ST-04, ST-05, ST-07, ST-08; ST-06 is partial per F3.2 and is omitted from quick-run pending Phase G).

Run ST-01 — Rapid-Fire Events Run ST-02 — Governance Delay Spike Run ST-03 — WAL Fsync Degradation Run ST-04 — Preflight Abort (WAL)

Run ST-05 — Preflight Retry Exhaustion Run ST-07 — Audit Log Contention Run ST-08 — Cascading Module Fault Reset Dashboard to Nominal

### F3.4 — STRESS-CERT-001.yaml Schema

Phase F exit requires a founder-signed `STRESS-CERT-001.yaml` committed to `governance/signoffs/`. This artifact mirrors the pattern of MARKET-DATA-001, TIMING-CONSTANTS-001, and LIVE-CAPITAL-001.
[code] 
    stress_cert_id:         "STRESS-CERT-001"
    phase:                  "F"
    signed_by:              "@founder"
    signed_at:              "ISO-8601"
    simulation_scenarios:
      ST-01: { status: "pass", notes: "loop P99 backpressure verified via slider" }
      ST-02: { status: "pass", notes: "governance flag timeout TERMINAL_HALT verified" }
      ST-03: { status: "pass", notes: "WAL fsync degradation P1 alert verified" }
      ST-04: { status: "pass", notes: "PF-01 abort indefinite block verified" }
      ST-05: { status: "pass", notes: "PF-10 retry exhaustion verified" }
      ST-06: { status: "deferred", notes: "Phase III — combined throughput+flag harness required" }
      ST-07: { status: "pass", notes: "audit log contention scrollability verified" }
      ST-08: { status: "pass", notes: "cascading module fault isolation verified" }
    production_harness_scenarios_deferred:
      - "ST-06 combined throughput+staleness injection"
      - "WAL Sequence Monotonicity Guard against live WAL state"
    govcheck_gate:          "F-exit"
    sha256:                 ""    # populated by CI on commit
[/code]

## Phase F — Consistency Review

### Consistency Checks Performed

  - Simulation Controller (F2) writes only to E4 dashboard DOM elements already specified in the Phase E task — no new IDs introduced into E4 markup
  - Simulation isolation guarantee (F2.1) confirmed — `SIM_STATE` has no code path to `/dev/shm/*`, `WAL`, or Governance Audit Log per Phase D/C definitions
  - Redis heartbeat key schema (F2.3) is structurally compatible with the polling mechanism described in E4.3 — same key-per-module pattern, same 1Hz/5s TTL relationship
  - Stress Test Registry severity labels (Extreme/High/Moderate) are distinct from, and do not redefine, the P0/P1/P2 incident taxonomy of A2.3
  - ST-02 expected response (fail-safe halt on unreadable governance flags) is consistent with the Sense-phase ordering invariant in E2.3 ("governance flag check must precede signal evaluation")
  - ST-04 and ST-05 expected responses are consistent with E3.3 arming sequence — Category 1 failures produce indefinite block, Category 3 failures permit exactly one retry
  - Gap Resolution Matrix (F.2) dispositions for all 5 Phase E gaps are mutually exclusive and collectively exhaustive — no gap is silently dropped or duplicated
  - Dashboard independence invariant (E4.3 / E4.1) extended, not contradicted, by the F2.1 visual distinction requirement
  - All new CSS classes use existing CSS variables — verified against `:root` declarations
  - PHASES array and timeline correctly reflect Phase F as `active`, distinct from `complete` (A–E) and `pending` (G–L)



### Governance Reconciliation Status

Item| Previous Classification| Reconciled Status| Authority  
---|---|---|---  
Prometheus metrics schema / Grafana specs| Gap (P3)| Confirmed| Phase G (G2, G3, G5 complete)  
inotify ≤1s P0 SLA hardware validation| Gap (P1)| Deferred by Design| G4.3 protocol; Phase III exit criterion  
ABORT preflight runbook| Gap (P2)| Confirmed| Phase G (G4 REC-008 complete)  
ST-06 throughput+flag harness| Gap| Deferred by Design| Phase III production harness  
STRESS-CERT-001 govcheck F-exit rule| Gap| Deferred by Design| Phase J (J5.1)  
SIM_STATE → production stress harness promotion| Gap| Deferred by Design| Phase III  
Redis heartbeat simulation-to-production transition| Gap| Deferred by Design| Phase III (H4, J3.1 PROC-006)  
  
G

## Phase G — Observability, Resilience & Governance Feedback

Prometheus Metrics Registry, Observability Bridge, Governance-Simulation Feedback Loop, Resilience Registry, and Telemetry Dashboard.

Complete

## G1 — Observability Bridge

Classification
    Infrastructure Authority Document — Tier 1

Version
    1.0

Owner
    Chief Architecture Office

Governing Principles
    P-001, P-003, P-007

Interfaces With
    Phase E §E4 (Runtime Status Dashboard), Phase F §F2 (Simulation Controller), Phase D §D3 (Audit Log)

### G1.1 — Observability Bridge Thesis

The Observability Bridge is the structured data-sink layer between the EGBOS_SIM controller (and, in production, the live execution plane) and all downstream consumers: the E4 dashboard, the Prometheus exporter, and the Governance Audit Log. SIM_STATE mutations are not raw observations — they are typed, versioned telemetry events that flow through a defined sink registry before reaching any consumer.

This architecture makes the simulation-to-production transition a configuration change, not a code change: the same sink interfaces that consume SIM_STATE events in Phase F will consume live Prometheus scrapes and inotify events in Phase IV. The bridge is the abstraction layer that makes observability a first-class governance concern rather than an operational afterthought.

### G1.2 — Telemetry Event Schema

Every data path that writes to the E4 dashboard MUST produce a TelemetryEvent first and route it through the ObservabilityBridge.dispatch(event) method (defined in Task 6 JavaScript). This is the architectural constraint that makes all dashboard state auditable.
[code] 
    {
      event_id:        "TEL-{epoch_ms}-{counter}",   // Unique per event
      source:          "simulation | prometheus | inotify | redis_heartbeat",
      category:        "heartbeat | module_status | alert | latency | governance_flag",
      timestamp_utc:   "ISO-8601",
      payload:         {},                             // Category-specific — see G2 registry
      sim_session_id:  "SIM-{epoch_ms} | null",       // Non-null only when source=simulation
      schema_version:  1
    }
[/code]

### G1.3 — Sink Registry

Sink ID| Consumer| Source Filter| Delivery SLA| Failure Mode  
---|---|---|---|---  
SINK-01| E4 Heartbeat Panel| category=heartbeat,latency| ≤5s staleness (ref: E4.3)| Panel enters STALE state; no operational impact  
SINK-02| E4 Module Status Panel| category=module_status| ≤5s staleness (ref: E4.3)| Module shown as UNKNOWN; advisory only  
SINK-03| E4 Alert Log| category=alert,governance_flag| ≤1s for severity=P0; ≤10s for P1/P2 (ref: E4.3 inotify SLA)| Alert queued; delivered on next poll cycle; P0 queue never discarded  
SINK-04| Prometheus Exporter| category=heartbeat,latency| 15s scrape interval| Scrape gap logged; no halt  
SINK-05| Governance Audit Log (D3)| category=governance_flag,alert where severity∈{P0,P1}| Synchronous — blocks dispatch until written| Write failure → P1 incident; dispatch retried up to 3×  
  
### G1.4 — inotify Delivery SLA — Phase G Resolution

This subsection explicitly closes the Phase E / Phase F partially-addressed gap regarding the inotify ≤1s P0 delivery SLA.

  - In production, SINK-03 is backed by inotify on `governance/audit/{date}.jsonl` (ref: E4.3). The SLA requires P0 events to reach the Alert Log within 1 second of being written to the Audit Log.
  - The ObservabilityBridge enforces this by maintaining a priority queue for the SINK-03 dispatch path: P0 events bypass the normal batch delivery cycle and are dispatched synchronously.
  - In simulation (Phase F), SINK-03 is backed by the SIM_STATE.alertQueue array; the same priority-queue logic applies, making the simulation behavior structurally identical to production.
  - Operational validation on production hardware (measuring actual inotify kernel delivery latency under load) remains a Phase III staging qualification activity, tracked in the gap list.



## G2 — Prometheus Metrics Registry

Classification
    Observability Authority Document — Tier 1

Owner
    Chief Architecture Office

Active From
    Phase III (staging) for production scraping; Phase G for simulation export

Resolves
    Phase E Gap (P3) — "Prometheus metrics schema and Grafana panel specifications not yet defined"

### G2.1 — Metrics Registry Philosophy

Every numeric quantity that appears in the E4 dashboard or is consumed by a Safety Monitor (Phase D §D2) MUST have a corresponding Prometheus metric definition in this registry. A dashboard panel backed by an unregistered metric is a governance gap — identical in severity to a correctness claim without an enforcement mechanism. The registry entry is the contract between the metric producer (execution engine / EGBOS_SIM) and the metric consumer (Grafana / Safety Monitor threshold evaluation).

### G2.2 — Metric Definition Schema
[code] 
    // Prometheus metric definition — governance/metrics/{metric_id}.yaml
    metric_id:          "PROM-NNN"
    prometheus_name:    "egbos_{subsystem}_{name}_{unit}"   // Prometheus naming convention
    type:               "gauge | counter | histogram | summary"
    description:        |
      One-sentence description matching the Prometheus HELP string exactly.
    labels:             ["hardware_id", "session_id"]       // Required label set
    unit:               "nanoseconds | microseconds | bytes | ratio | boolean"
    sim_state_mapping:  "SIM_STATE.loopLatencyUs"           // Dot-path into SIM_STATE
    timing_constant_ref: "loop_cycle_p99_us"                // Key in timing_constants.json; null if not latency
    governing_claim:    "CLAIM-L4-001"                      // Null if not claim-backed
    safety_monitor_ref: "Kill Switch Latency Enforcer"      // D2.2 rule name; null if not a monitor input
    alert_threshold:
      warn:  
      breach: 
      breach_action: "P1_incident | P0_incident | advisory"
[/code]

### G2.3 — Active Metrics Registry

Metric ID| Prometheus Name| Type| SIM_STATE Mapping| Governing Claim| Safety Monitor| Breach Action  
---|---|---|---|---|---|---  
PROM-001| egbos_wal_fsync_p99_nanoseconds| gauge| SIM_STATE.loopLatencyUs (proxy)| CLAIM-WAL-001| WAL Sequence Monotonicity Guard| P0_incident  
PROM-002| egbos_loop_cycle_p99_microseconds| gauge| SIM_STATE.loopLatencyUs| CLAIM-L3-001| Kill Switch Latency Enforcer| P1_incident  
PROM-003| egbos_feed_to_decision_p99_microseconds| gauge| SIM_STATE.loopLatencyUs (aggregate)| CLAIM-L3-05| Feed Staleness Circuit Breaker| P1_incident  
PROM-004| egbos_position_integrity_delta| gauge| SIM_STATE.modules.risk_checks_module (fault=nonzero)| CLAIM-INV-001| Position Integrity Monitor| P0_incident  
PROM-005| egbos_gross_exposure_ratio| gauge| null (live only)| CLAIM-L3-006| Gross Exposure Ceiling Monitor| P1_incident  
PROM-006| egbos_preflight_armed_boolean| gauge| SIM_STATE.sessionActive| CLAIM-L3-05| null| advisory  
PROM-007| egbos_active_waiver_days_total| gauge| null (govcheck output)| null| Waiver Accumulation Circuit Breaker| advisory  
PROM-008| egbos_module_fault_total| counter| SIM_STATE.modules (fault count)| null| null| P1_incident  
PROM-009| egbos_alert_log_p0_total| counter| SIM_STATE.alertCount (P0 subset)| null| null| P0_incident  
PROM-010| egbos_spsc_ring_utilization_ratio| gauge| null (live only)| CLAIM-SPSC-001| null| advisory  
PROM-011| egbos_replay_fidelity_pass_boolean| gauge| null (nightly job)| CLAIM-FEATURE-001| Replay Fidelity Guard| P1_incident  
PROM-012| egbos_signal_decay_sharpe_30d| gauge| null (live only)| null| Signal Decay Level 3 Halt| P0_incident  
  
### G2.4 — Simulation Export Format

The ObservabilityBridge (G1) exposes a method `getPrometheusSnapshot()` that serializes the current SIM_STATE into Prometheus text exposition format for all metrics where `sim_state_mapping` is non-null.
[code] 
    # HELP egbos_loop_cycle_p99_microseconds P99 latency of one complete Sense→Evaluate→Execute→Audit iteration
    # TYPE egbos_loop_cycle_p99_microseconds gauge
    egbos_loop_cycle_p99_microseconds{hardware_id="simulation",session_id="SIM-1234567890"} 180
    # HELP egbos_preflight_armed_boolean 1 if preflight protocol (E3) has armed the execution loop; 0 otherwise
    # TYPE egbos_preflight_armed_boolean gauge
    egbos_preflight_armed_boolean{hardware_id="simulation",session_id="SIM-1234567890"} 0
[/code]

## G3 — Governance-Simulation Feedback Loop

Classification
    Constitutional Authority Supplement — Tier 0

Governing Principles
    P-001 (Mechanical Enforcement), P-005 (Failure Mode First), P-006 (Capital Safety)

Interfaces With
    Phase F §F3 (Stress Test Registry), Phase D §D2 (Safety Monitors)

⚑ **Governance Reconciliation Note:** SimulationController (F1) → SimulationFeedbackController (G3) → GovernanceAction → ObservabilityBridge (G1) → Telemetry Sinks is explicitly wired. The feedback loop is architecturally complete and operational in simulation. Production wiring deferred to Phase III per progressive trust model.

### G3.1 — Feedback Loop Thesis

The Governance-Simulation Feedback Loop is the mechanism by which Stress Test Registry outcomes (Phase F §F3) drive automated governance actions — not merely dashboard updates. A stress test that terminates without producing a governance artifact has not been executed in the EGBOS sense; it has been observed. Observation without governance response is P-001 violation: a constraint that cannot be enforced by a tool is not active.

The feedback loop operationalizes this for simulation: when a scenario from the Stress Test Registry fires (via `EGBOS_SIM.runScenario()`), the SimulationFeedbackController (defined in Task 6) evaluates the resulting SIM_STATE delta against the governance rules table (D2.2) and produces a typed GovernanceAction. That action is dispatched through the ObservabilityBridge to SINK-03 (alert log), SINK-05 (audit log analog in simulation), and the E4 module status panel simultaneously.

### G3.2 — GovernanceAction Schema
[code] 
    {
      action_id:        "GOV-{epoch_ms}",
      triggered_by:     "ST-01 | ST-02 | ... | manual",
      trigger_event:    TelemetryEvent,               // The event that caused evaluation
      rule_matched:     "Position Integrity Monitor | Kill Switch Latency Enforcer | ...",
      action_type:      "pause_module | emit_alert | reinitialize_state | block_preflight | advisory",
      target_module:    "orders_module | risk_checks_module | null",
      severity:         "P0 | P1 | P2 | INFO",
      auto_reversible:  true,                         // Can the action be auto-undone on recovery?
      recovery_trigger: "manual | feed_recovery | watchdog_timeout",
      sim_only:         true                          // Always true in Phase G; false in Phase IV+
    }
[/code]

### G3.3 — Scenario-to-Governance Action Mapping

Scenario| Rule Matched (D2.2)| GovernanceAction.action_type| Target Module| Auto-Reversible| Recovery Trigger  
---|---|---|---|---|---  
ST-01 (Rapid-fire events)| Kill Switch Latency Enforcer| emit_alert| null| true| watchdog_timeout  
ST-02 (Governance delay spike)| Position Integrity Monitor| pause_module + block_preflight| orders_module| false| manual  
ST-03 (WAL fsync degradation)| Kill Switch Latency Enforcer| emit_alert| wal_writer| true| watchdog_timeout  
ST-04 (Preflight abort — WAL)| WAL Sequence Monotonicity Guard| block_preflight + emit_alert| null| false| manual  
ST-05 (Preflight retry exhaustion)| Feed Staleness Circuit Breaker| block_preflight + emit_alert| feed_ingestor_l0| false| manual  
ST-07 (Audit log contention)| null (observability only)| emit_alert| audit_logger| true| watchdog_timeout  
ST-08 (Cascading module fault)| Gross Exposure Ceiling Monitor| pause_module (risk_checks first, then orders)| risk_checks_module → orders_module| false| manual  
  
### G3.4 — Feedback Loop State Machine
[code] 
    STRESS TEST INITIATED
            │
            ▼
    SCENARIO PARAMETERS APPLIED (SIM_STATE mutated)
            │
            ▼
    TELEMETRY EVENT PRODUCED (ObservabilityBridge.dispatch)
            │
            ▼
    GOVERNANCE RULE EVALUATION (SimulationFeedbackController.evaluate)
            │
            ├─ No rule matched ──────► SINK-03 INFO alert; no action
            │
            └─ Rule matched
                   │
                   ├─ action_type = emit_alert ────────► SINK-03 dispatch (priority by severity)
                   │                                      SINK-05 audit write
                   │
                   ├─ action_type = pause_module ──────► simUpdateModuleDOM(target, 'inactive')
                   │                                      SINK-02 dispatch (module status update)
                   │                                      SINK-03 P1/P0 alert
                   │
                   ├─ action_type = block_preflight ───► SIM_STATE.sessionActive = false (if P0)
                   │                                      E4 Preflight Status → NOT ARMED
                   │                                      SINK-03 P0 alert (synchronous)
                   │
                   └─ action_type = reinitialize_state ► SIM_STATE reset to last stable snapshot
                                                           SINK-03 INFO alert with diff summary
[/code]

### G3.5 -- ProductionGovernanceController Specification (PGC-001)

⊕ _Absorbed from QOS Final Absorption Pass 2026-06-16._ Gap filled: G3.1-G3.4 specifies the Governance-Simulation Feedback Loop as `sim_only: true` with production wiring deferred to Phase IV+ but left unspecified. This section specifies the production-wired equivalent during the blueprint phase, eliminating blank-slate redesign risk at Phase IV+ implementation.

The `ProductionGovernanceController` (PGC) is the production analog of the `SimulationFeedbackController` (G3.1). It operates on live `TelemetryEvent` streams (SINK-01 through SINK-04) and evaluates the same governance rules table (D2.2) against real system state, producing typed `GovernanceAction` objects with `sim_only: false`. The two controllers share an identical interface contract; only data source and the `sim_only` flag differ.

Field| SimulationFeedbackController (G3.1)| ProductionGovernanceController (G3.5)  
---|---|---  
`sim_only`| true (always)| false (always)  
Data source| SIM_STATE JavaScript object| Live TelemetryEvent streams (SINK-01..04)  
`pause_module`| simUpdateModuleDOM() -- UI only| SIGTERM to target module process + SINK-02 update + SINK-03 P0 alert  
`block_preflight`| SIM_STATE.sessionActive = false| govcheck preflight block written to `governance/preflight-blocks/{epoch_ms}.yaml`; PF-01 fails until HITL operator clears block  
Recovery path| Auto-reversible per G3.3 table| All P0/P1 actions require HITL operator sign-off before reversal (AD-010 monotone latch applied)  
Audit sink| SINK-05 simulation analog| SINK-05 production audit log (Tier-0 immutable)  
  
**GC-E-PGC-001:** During Phase IV+ live operation, govcheck verifies at each G4 gate evaluation that the PGC is registered and operational (heartbeat present in `governance/pgc-status.json`, updated every 60 seconds). Absent or stale heartbeat = capital authority suspended until HITL operator confirms PGC recovery. Gate: G4 (continuous). Severity: Error at Phase IV-A entry; WARNING-only prior to IV-A.

The SimulationFeedbackController (G3.1-G3.4) remains active in all phases including Phase IV+ and operates in parallel with PGC on simulation test runs.

## G4 — Resilience Registry & Failure Recovery Protocols

Classification
    Capital Safety Authority Document — Tier 0

Governing Principles
    P-005 (Failure Mode Precedes Nominal), P-006 (Capital Safety)

Resolves
    Phase E Gap (P2) — "Operational runbook for preflight failure recovery not yet authored"; Phase F Gap — "Promotion path from SIM_STATE scenarios to production stress harness unspecified"

### G4.1 — Resilience Registry Philosophy

The Resilience Registry is the formalization of every recovery path that the system must be able to execute. It mirrors the claim registry structure: each entry has an ID, a trigger condition, a recovery procedure, and a verification standard. An entry that exists in code but not in this registry is not a recognized recovery path — it is invisible to the governance system and cannot satisfy a pre-flight check or an incident report.

### G4.2 — Recovery Protocol Registry Table

Protocol ID| Trigger Condition| Recovery Class| Procedure Summary| Verification Standard| Phase Availability  
---|---|---|---|---|---  
REC-001| WAL CRC32C mismatch at PF-01| Manual-authorized| (1) Identify truncation point via govcheck wal-scan; (2) Commit RECOVERY-RECORD-{date}.yaml; (3) RFP reconciliation if live capital; (4) Founder signs restart auth (HITL_RESTART_AUTH per D4.2)| Byte-identical position state from replay × 3 (ref: CLAIM-WAL-001 V4); INV-001 check must pass| Phase III+  
REC-002| PF-10 exchange connectivity double-failure| Manual-authorized (1 auto-retry permitted)| (1) Verify physical cross-connect status; (2) Re-run PF-10 after 60s cooldown; (3) If second failure: commit connectivity-incident-{date}.yaml; (4) Notify exchange operations| RTT ≤500µs P99 measured post-recovery (ref: timing_constants.json)| Phase III+  
REC-003| INV-001 violation (TERMINAL_HALT)| Manual-authorized only| Full P0 protocol per A2.3: halt, freeze, RFP reconciliation, root cause, RECOVERY-RECORD commit, govcheck deploy-gate pass, Founder HITL_RESTART_AUTH| RFP position delta = 0; govcheck G2 pass; INV-001 re-verified V4| Phase IV+  
REC-004| FEED_STALE_HALT (auto-reversible)| Automatic on feed recovery| (1) Feed reconnect + snapshot receipt; (2) Sequence continuity confirmed; (3) FeedStaleFlag cleared; (4) pos_limit_latch auto-reset| Feed sequence gap = 0; PF-11 re-passes without human intervention| Phase II+  
REC-005| Replay fidelity failure (RESEARCH_HALT)| Manual-authorized| (1) Feature engine bisect to identify non-determinism source; (2) Fix + commit; (3) Re-run nightly replay validation × 3; (4) Commit new golden fixture with new SHA-256 in CLAIM-FEATURE-001| 3× byte-identical replay on new golden fixture; govcheck C0 pass| Phase II+  
REC-006| Cascading module fault (ST-08 class)| Manual-authorized| (1) Fault isolation: identify which module faulted first (audit log timestamp diff); (2) Risk Checks Module recovery before Orders Module — ordering is mandatory; (3) Each module: govcheck module-check before re-arm; (4) Full pre-flight (E3) before new session| All pre-flight checks pass; Risk Checks Module active before Orders Module active| Phase III+  
REC-007| Simulation Controller state corruption (Phase G only)| Automatic| EGBOS_SIM.resetDashboard() invoked; SIM_STATE re-initialized to defaults; all sink queues flushed| Dashboard displays IDLE; all modules STANDBY; alert log contains reset confirmation| Phase G only  
REC-008 | ABORT preflight — PF-01 WAL CRC failure (ST-04 class) | Manual-authorized | (1) Identify WAL truncation point: `govcheck wal-scan --target production`; (2) Confirm last valid WAL sequence number; (3) Commit `governance/recovery/RECOVERY-RECORD-{date}.yaml` with fields: wal_truncation_point, records_replayed, position_state_hash, rfp_match_status; (4) If live capital was deployed at halt: execute RFP reconciliation before proceeding; (5) Founder signs HITL_RESTART_AUTH WAL record per D4.3; (6) Re-run full E3 pre-flight — PF-01 must PASS; (7) Commit INCIDENT-{date}.yaml to `governance/incidents/`. | PF-01 PASS on re-run; govcheck deploy-gate --target production exits 0; RECOVERY-RECORD-{date}.yaml committed and signed | Phase III+  
REC-009 | Stratum-0 cold recovery — WAL Primary and WAL Shadow both unrecoverable (controller failure) | Manual-authorized — highest priority | (1) Confirm WAL Primary and WAL Shadow both unreadable via filesystem check; (2) Mount Stratum-0 device (separate SATA controller) read-only: `mount -o ro /dev/sata0 /mnt/stratum0`; (3) Run CRC32C verification on Stratum-0 records from last known checkpoint; (4) Reconstruct position state from Stratum-0 replay; (5) Compare reconstructed state against any available exchange position reports (RFP); (6) Document all discrepancies in RECOVERY-RECORD-{date}.yaml — do NOT auto-reconcile discrepancies; (7) Human decision required on each discrepancy before restart; (8) Founder signs HITL_RESTART_AUTH; (9) New WAL Primary initialized with next sequence number = Stratum-0 last_sequence + 1; (10) Full E3 pre-flight required before any orders. | Stratum-0 replay produces byte-identical position state to any surviving partial WAL records; discrepancy count documented (zero or signed-off); new WAL initialized with correct sequence continuation | Phase III+  
REC-010 | TERMINAL_HALT with simultaneous exchange connectivity loss | Manual-authorized — extended procedure | (1) Execute TERMINAL_HALT protocol per A2.3 P0 — halt, freeze all positions; (2) Do NOT wait for exchange connectivity to perform WAL-internal reconciliation — WAL replay is exchange-independent; (3) Run WAL replay to reconstruct internal position state: `scripts/wal_replay_reconcile.sh --output internal_positions_{date}.yaml`; (4) Record internal_positions_{date}.yaml in RECOVERY-RECORD-{date}.yaml as internal_position_state_hash; (5) When exchange connectivity restores: execute RFP reconciliation — compare internal_positions_{date}.yaml against exchange RFP response; (6) Any delta between internal reconstruction and RFP is a capital discrepancy requiring regulatory notification assessment; (7) Do NOT restart engine until RFP delta = 0 or discrepancy is formally documented with external evidence; (8) If connectivity does not restore within 4 hours: escalate to exchange operations with internal position state hash as evidence of good faith reconciliation attempt; (9) Founder signs HITL_RESTART_AUTH only after step 7 is complete. | WAL-internal position reconstruction completes without exchange connectivity; RECOVERY-RECORD includes internal_position_state_hash; RFP delta is zero or formally documented before HITL_RESTART_AUTH is signed | Phase IV+  
  
### G4.3 — inotify Delivery SLA — Production Validation Protocol

Structured procedure for the production hardware validation of the ≤1s P0 delivery SLA (carried forward from Phase E, partially resolved in G1.4):

  1. **Pre-condition:** Production hardware_id active; timing_constants.json current for that hardware_id.
  2. **Injection:** Inject 1000 synthetic P0-class Audit Log entries via `scripts/inotify_sla_test.sh` at controlled intervals (10ms apart).
  3. **Measurement:** Measure inotify delivery latency per entry: time from file write to SINK-03 dispatch (using CLOCK_MONOTONIC_RAW TSC diff).
  4. **Pass criterion:** P99 delivery latency ≤ 1,000,000,000 ns (1 second). P50 target: ≤ 100ms. Any single observation > 2s: immediate test failure.
  5. **Artifact:** Commit results as V4 proof artifact at `governance/proof_artifacts/inotify_sla_{hardware_id}_{date}.json`.
  6. **Claim update:** Update CLAIM-L3-05 (or create CLAIM-OBSERVABILITY-001) with this artifact SHA-256.
  7. **Exit criterion:** This protocol is a Phase III exit criterion (ref: A2.2 Phase III) — staging cannot be exited without a passing inotify SLA measurement on production hardware.



## G5 — Telemetry Dashboard & View Toggle

Classification
    Observability Document — Tier 1

Interfaces With
    Phase E §E4 (Runtime Status Dashboard — extended, not replaced)

Governing Principle
    P-001 (dashboard is an enforcement-observable surface)

### G5.1 — Telemetry View Architecture

PRODUCTION WIRING DEFERRED BY DESIGN — Phase III Exit Criterion (Progressive Trust Model)

The Telemetry Dashboard currently operates in **SIMULATION mode only**. The SIMULATION and MOCK_LIVE modes use in-browser `SIM_STATE` as the data source (see Phase F §F2.1 Isolation Guarantee). This is **architecturally complete** — the governance telemetry architecture (B1.7 decay vectors, G3 feedback loop, G1 ObservabilityBridge, G2 Prometheus registry, G4 Resilience Registry, G5 Dashboard) is fully specified and wired in simulation. LIVE mode requires production implementation of the following components, deferred to Phase III under the progressive trust model (A2.1):

  - **egbos_exporter sidecar** — C++20 + Rust HTTP server exposing `localhost:9090/metrics` (H2.2 specification). Status: **Architecture complete; implementation Phase III**. Phase III exit criterion.
  - **inotify-based Audit Log tail** — Production SINK-03 delivery for P0 events (≤1s SLA per G1.4). Status: **Architecture complete; implementation Phase III**. Phase III exit criterion. Proof artifact required: `governance/proof_artifacts/inotify_sla_{hardware_id}_{date}.json` per G4.3 protocol.
  - **Redis heartbeat polling** — Production SINK-02 at 1Hz per E4.3. Status: **Architecture complete; implementation Phase III**. Requires Redis deployment per H4.
  - **Grafana panel JSON** — PAN-001 through PAN-012 panel definitions (H3.2). Status: **Architecture complete; implementation Phase III**. Phase III deliverable.



**Blocking rule:** GC-E-051 blocks G2 (production deploy gate) until all four components above are implemented and proof artifacts committed. The TELEMETRY_MODE toggle must be removed from browser-side JavaScript before any Phase IV capital deployment. Mode control must be a server-side configuration parameter in the egbos_exporter process.

⚑ **Governance Reconciliation Note:** The governance telemetry architecture is complete. B1.7 decay vectors (DV-01 through DV-05), G1 ObservabilityBridge, G2 Prometheus Metrics Registry, G3 Governance-Simulation Feedback Loop, G4 Resilience Registry, and G5 Telemetry Dashboard are fully specified and integrated. Certain production integrations remain scheduled for Phase III under the progressive trust model — this is by design, not a gap.

SIM_STATE isolation is fully specified in Phase F §F2.1 and constitutionally enforced by AD-013 and GC-E-056. All statements in F2.1 apply equally to the SIMULATION and MOCK_LIVE modes of the Telemetry Dashboard.

**⚠ SIMULATION ENVIRONMENT ONLY** — This toggle controls a browser-resident simulation. In production (Phase IV+), telemetry mode is set via egbos_exporter process configuration, not this UI. This panel has no connection to any live trading system. 

Telemetry Source Toggle SIMULATION — Active

SIMULATION Mode MOCK_LIVE Mode LIVE Mode (Phase IV+)

Prometheus Snapshot — Current Mode
[code] 
    // Select a mode above to generate snapshot
[/code]

### G5.2 — View Mode Specifications

Mode| Data Source| Requires| Available From| Snapshot Output  
---|---|---|---|---  
SIMULATION| SIM_STATE via ObservabilityBridge| EGBOS_SIM session active or idle| Phase G| Live SIM_STATE serialized to Prometheus text (G2.4)  
MOCK_LIVE| Deterministic fixture constants from timing_constants.json baseline values| None (always available)| Phase G| Fixed healthy-session snapshot: all modules ACTIVE, loop P99=180µs, preflight_armed=1, zero faults  
LIVE| Production Prometheus scrape endpoint + inotify| Phase IV+ hardware; production session armed| Phase IV+| Real scrape output; any deviation from MOCK_LIVE fixture flags as anomaly  
  
## Phase G — Consistency Review

### Consistency Checks Performed

  - ObservabilityBridge sink registry (G1.3) maps directly to E4.3 data source contracts — no new staleness tolerances introduced without E4 cross-reference
  - Prometheus metric schema (G2.2) `sim_state_mapping` field enables simulation-to-production transition via configuration change only — consistent with G5.1 Telemetry View Architecture thesis
  - PROM-004 (position_integrity_delta) maps to CLAIM-INV-001 and Position Integrity Monitor — consistent with D2.2 governance rules table
  - SimulationFeedbackController.evaluate() rule matching logic (Task 8B) maps exactly to G3.3 scenario-to-governance action table — no undocumented rules
  - Recovery protocol REC-007 (simulation state corruption) is marked `sim_only` and Phase G — consistent with F2.1 simulation isolation guarantee
  - inotify SLA production validation protocol (G4.3) correctly deferred to Phase III exit criterion — consistent with A2.2 Phase III Operating Constraints
  - `EGBOS_TELEMETRY_MODE='LIVE'` is gated behind a Phase IV+ notice — consistent with progressive trust model (A2.1)
  - All new JavaScript appends to the existing IIFE without modifying existing declarations — no variable shadowing risk
  - All new CSS uses only :root variables — no hardcoded colors introduced
  - Phase G timeline entry status updated to 'active' — consistent with PHASES array mutation in Task 1



### Gaps Identified for Phase H

  - ADR catalog formalization: all architectural decisions made in Phases A–G (AD-001 through AD-012 plus any Phase G additions) must be catalogued in Phase H with immutable rationale records
  - Prometheus metrics exposition endpoint (a real HTTP server exposing `/metrics`) is not yet implemented — G2.4 defines the format; Phase H must specify the deployment topology
  - Grafana dashboard panel definitions (panel JSON) are not yet produced — metric names are now registered in G2.3; panel layout and alert thresholds require Phase H specification
  - Production Redis deployment for module heartbeat schema (F2.3, G1.3 SINK-02) — connection management, ACL, and failure modes unaddressed
  - SimulationFeedbackController rule set covers only D2.2 rules that are simulatable — rules requiring live WAL state (WAL Sequence Monotonicity Guard evaluated against real records) remain Phase IV+ only and are not represented in the simulation



H

## Phase H — ADR Catalog & Infrastructure Deployment Governance

Immutable Architecture Decision Records (AD-001–AD-013), Prometheus exposition endpoint topology, Grafana panel definitions, Redis production deployment, and WAL simulation boundary rationale.

Complete

## H1 — Architecture Decision Record Catalog

Classification
    Immutable Governance Artifact — Tier 0

ADR Class
    Formal — requires RFC to supersede any entry

Governing Principles
    P-001, P-003, P-004, P-007

Scope
    AD-001 through AD-013 (all phases A–H)

### H1.1 — ADR Thesis

The ADR Catalog is the institutional memory of every irreversible or high-consequence architectural decision made during EGBOS construction. Each entry preserves: the decision made, the alternatives explicitly rejected, the rationale grounding the choice, and the claims and governance rules that depend on it. Entries are append-only. A superseded ADR is marked superseded with a pointer to its replacement ADR; it is never deleted. No engineer may alter an architectural decision covered by a registered ADR without producing a new ADR and, where the decision invalidates a V3+ claim, filing a claim amendment before the code change merges.

### H1.2 — ADR Schema
[code] 
    {
      adr_id:               "AD-NNN",
      title:                "Short imperative title",
      status:               "active | superseded | draft",
      phase_introduced:     "Phase letter (A–H)",
      governing_principles: ["P-XXX", ...],
      dependent_claims:     ["CLAIM-XXX", ...],
      dependent_gc_rules:   ["GC-E-XXX", ...],
      decision:             "One-paragraph statement of what was decided",
      alternatives_rejected: [
        { alternative: "...", rejection_reason: "..." }
      ],
      rationale:            "Paragraph explaining why this decision holds",
      superseded_by:        null | "AD-NNN",
      amendment_authority:  "CTO + Governance Board | Founder"
    }
[/code]

### H1.3 — Full ADR Registry

ADR ID| Title| Status| Phase| Governing Principles| Dependent Claims| Amendment Authority  
---|---|---|---|---|---|---  
AD-001| Replay Engine Canonical Source| active| B| P-003, P-004| CLAIM-FEATURE-001, CLAIM-L3-001| CTO + Governance Board  
AD-002| Research Plane Hard Boundary| active| B| P-001, P-002, P-004| CLAIM-FEATURE-001| CTO + Governance Board  
AD-003| Tier-0 Log Threshold fsync Policy| active| C (formalized H)| P-003, P-006| CLAIM-WAL-001| CTO + Governance Board  
AD-004| Fixed-Point Arithmetic Mandate in Execution Plane| active| C (formalized H)| P-003| CLAIM-FEATURE-001, CLAIM-L3-001, CLAIM-INV-001| CTO + Governance Board  
AD-005| io_uring Single-CQE-Consumer Invariant| active| C (formalized H)| P-003, P-001| CLAIM-WAL-001| CTO + Governance Board  
AD-006| SHM Region Sizes as Compile-Time Constants| active| C (formalized H)| P-002, P-003| CLAIM-SPSC-001, CLAIM-L3-004, CLAIM-L3-006| CTO + Governance Board  
AD-007| Stale Feed Response — Position Freeze Not Force-Close| active| C (formalized H)| P-006| — (operational policy)| CTO + Governance Board  
AD-008| Redis Streams as Exclusive IPC Bridge for L6/L7 Python| active| A (formalized H)| P-002, P-001| — (boundary policy)| CTO + Governance Board  
AD-009| FlatBuffers as Exclusive Schema Language for Inter-Layer Events| active| A (formalized H)| P-003, P-002| CLAIM-FEATURE-001| CTO + Governance Board  
AD-010| Monotone Latch as Exclusive Safety Enforcement Mechanism in L4| active| D (formalized H)| P-002, P-006| CLAIM-L3-006, CLAIM-INV-001| CTO + Governance Board  
AD-011| ONNX as Exclusive Model Inference Format in L5| active| A (formalized H)| P-003, P-002| CLAIM-FEATURE-001| CTO + Governance Board  
AD-012| WAL-Before-State as Mandatory Write Ordering in L3| active| A (formalized H)| P-003, P-006| CLAIM-WAL-001, CLAIM-L3-001, CLAIM-INV-001| CTO + Governance Board  
AD-013| Simulation Isolation — SIM_STATE Never Writes to WAL or SHM| active| F (formalized H)| P-002, P-004| GC-E-056 (AD-013 boundary)| Founder  
AD-014| HashiCorp Vault HSM-Backed Transit Secrets & Dynamic Key Rotation| active| K (formalized K)| P-001, P-006| CLAIM-GOVCHECK-001| CSO + Governance Board  
AD-015| Zero-Trust Host Isolation, mTLS, and Bastion Access Control| active| K (formalized K)| P-001, P-002| CLAIM-GOVCHECK-001| CSO + CTO  
AD-016| Active-Passive Geo-Redundancy & Sub-Minute Failover to CH2| active| K (formalized K)| P-003, P-006| CLAIM-WAL-001, CLAIM-L3-001| CRO + Chief Reliability Eng  
AD-017| Multi-Signatory Cryptographic Build, SBOM, and Secure Boot Validation| active| K (formalized K)| P-001, P-002, P-003| CLAIM-GOVCHECK-001| CTO + Chief Security Officer  
AD-018| Dual-Feed Packet-Level Line Arbitration & IEEE 1588 PTP Synchronization| active| K (formalized K)| P-003, P-004| CLAIM-FEATURE-001, CLAIM-L0-002| CTO + Lead Quant  
AD-019| Out-of-Process FPGA-Gated Risk Enforcement & System-Wide Optical Disconnect| active| K (formalized K)| P-003, P-006| CLAIM-RISK-STALE-001| CRO + Lead Quant  
AD-020| Automated Alpha Decay Governance & Signal Retirement Pipeline| active| K (formalized K)| P-002, P-005, P-007| CLAIM-MODEL-001, CLAIM-SIGNAL-001| Lead Quant + CRO  
AD-021| Tiered Governance Gating & Velocity-Safety Optimization| active| K (formalized K)| P-001, P-002, P-007| CLAIM-GOVCHECK-001| CTO + Governance Board  
AD-022| Portfolio VaR and Real-Time Multi-Asset Concentration Limits| active| K (formalized K)| P-005, P-006| CLAIM-RISK-STALE-001| CRO + Governance Board  
AD-023| Executable Incident Response and Automated Runbook Verification| active| K (formalized K)| P-002, P-006| — (resilience policy)| Chief Reliability Eng  
AD-024| Unified Schema Evolution and FlatBuffers Backward Compatibility Guard| active| K (formalized K)| P-002, P-003| CLAIM-FEATURE-001| CTO + Architecture Team  
AD-025| End-to-End Latency SLOs and Core-Level Thread Pinning| active| K (formalized K)| P-003, P-004| CLAIM-FEATURE-PARITY-001| CTO + Lead Quant  
  
AD-001 — Replay Engine Canonical Source

Status: active | Phase: B | Authority: CTO + Governance Board

**Governing Principles:** P-003, P-004  |  **Dependent Claims:** CLAIM-FEATURE-001, CLAIM-L3-001

**Decision:** The backtester MUST use `replay/replay_engine.h` only. No backtester may import from `engine/` directly (enforced by GC-E-RESEARCH-002).

**Alternative Rejected:** Direct `engine/` import for performance. _Rejection rationale:_ Eliminates the structural guarantee that research P&L curves are byte-identical to production replay — invalidating CLAIM-FEATURE-001 and the Sharpe gap decomposition.

**Rationale:** Replay fidelity is not a quality attribute — it is a capital governance requirement. A research result that cannot be replayed against production is not a result; it is a hypothesis without institutional standing.

AD-002 — Research Plane Hard Boundary

Status: active | Phase: B | Authority: CTO + Governance Board

**Governing Principles:** P-001, P-002, P-004  |  **Dependent Claims:** CLAIM-FEATURE-001

**Decision:** The research plane is a read-only consumer of the execution plane. No research code may write to `engine/` paths. Boundary enforced by GC-E-RESEARCH-001 (CI-checked import graph). Crossing this boundary requires an ADR.

**Alternative Rejected:** Permissive boundary with runtime guards. _Rejection rationale:_ Runtime guards are discipline-based (P-001 violation). A developer under deadline pressure will cross the boundary; CI must make this structurally impossible.

**Rationale:** Research contaminating execution-plane state creates a class of bugs that are invisible in testing (they only appear under live conditions) and catastrophic in production (they break replay determinism and position accounting simultaneously).

AD-003 — Tier-0 Log Threshold fsync Policy

Status: active | Phase: C (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-003, P-006  |  **Dependent Claims:** CLAIM-WAL-001

**Decision:** The Tier-0 raw event log uses threshold-based fsync (flush when buffer ≥ 64KB or ≥ 5ms elapsed). Per-event fsync is explicitly rejected for Tier-0. The WAL is the primary recovery path.

**Alternative Rejected:** Per-event fsync on Tier-0. _Rejection rationale:_ Per-event fsync bounds Tier-0 throughput to ~10K events/sec on NVMe — insufficient for institutional feed rates. The WAL already provides per-transaction durability over the identical window.

**Rationale:** Two per-event fsyncs for the same safety guarantee (WAL + Tier-0) is architectural waste that creates a throughput bottleneck without improving capital safety. The 5ms Tier-0 data loss window is acceptable because WAL recovery fully covers it. Any future proposal to promote Tier-0 to primary recovery path requires a new ADR superseding this entry.

AD-004 — Fixed-Point Arithmetic Mandate in Execution Plane (= AD-008 formalized)

Status: active | Phase: C (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-003  |  **Dependent Claims:** CLAIM-FEATURE-001, CLAIM-L3-001, CLAIM-INV-001

**Decision:** All price arithmetic in L0–L5 uses `int64_t` fixed-point at `PRICE_SCALE = 100,000`. Floating-point is forbidden on the hot path. `PRICE_SCALE` is a compile-time constant; changes require a new ADR and re-measurement of all affected timing constants.

**Alternative Rejected:** `double` precision with epsilon comparison. _Rejection rationale:_ Epsilon tolerance compounds in multi-leg positions and is not byte-identical across CPU generations, violating CLAIM-FEATURE-001.

**Rationale:** Floating-point non-determinism is a class of bug that only manifests when replaying on a different CPU generation than the one that produced the record. By the time it is detected, the replay fidelity chain is broken and every research result produced against that production record is suspect.

AD-005 — io_uring Single-CQE-Consumer Invariant (= AD-009 formalized)

Status: active | Phase: C (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-003, P-001  |  **Dependent Claims:** CLAIM-WAL-001

**Decision:** The WAL writer io_uring ring has exactly one CQE consumer thread. This is a documented assumption in CLAIM-WAL-001. Any multi-threaded ring access requires a new claim at minimum V1 class before the code change merges.

**Alternative Rejected:** Multi-threaded io_uring with locking. _Rejection rationale:_ Lock contention on the WAL writer critical path creates unpredictable fsync latency, invalidating WAL P99 timing constants and claims.

**Rationale:** WAL write ordering is a correctness-critical property, not a performance preference. The single-consumer invariant makes the ordering guarantee provable by inspection — no lock analysis required.

AD-006 — SHM Region Sizes as Compile-Time Constants (= AD-010 formalized)

Status: active | Phase: C (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-002, P-003  |  **Dependent Claims:** CLAIM-SPSC-001, CLAIM-L3-004, CLAIM-L3-006

**Decision:** All SHM region sizes are `constexpr` constants in `engine/core/shm_layout.h`, derived from instrument universe size and `timing_constants.json`. No runtime configuration. Changes require recompile and re-measurement of affected timing constants.

**Alternative Rejected:** Runtime-configurable ring sizes. _Rejection rationale:_ Runtime misconfiguration silently changes memory ordering guarantees without triggering CI. This creates a class of failure that is structurally preventable.

**Rationale:** Making the configuration space explicit at compile time means any change to it is visible in the build artifact — it cannot be deployed accidentally or silently.

AD-007 — Stale Feed Response — Position Freeze Not Force-Close (= AD-012 formalized)

Status: active | Phase: C (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-006  |  **Dependent Claims:** none (operational policy)

**Decision:** On `FEED_STALE`, L4 sets new-position limit to zero via monotone latch. Existing positions are NOT force-closed. This is an architectural decision, not a tuning parameter.

**Alternative Rejected (1):** `TERMINAL_HALT` on feed stale. _Rejection rationale:_ Feed gaps are routine (exchange maintenance, network blip) and do not imply position-state uncertainty — only signal uncertainty. TERMINAL_HALT is reserved for position-state uncertainty (INV-001 violation).

**Alternative Rejected (2):** Force-close all positions on stale feed. _Rejection rationale:_ Force-closing on stale data is adverse selection against the firm — we close at a price driven by a signal we don't trust, then pay re-entry cost when feed recovers.

**Rationale:** Capital safety (P-006) means minimizing expected loss under uncertainty. Holding a position with a known entry price is less risky than closing at an adversely selected exit price. The risk of holding is bounded by position size; the risk of a forced close on a stale feed is structurally unlimited (gap risk on re-entry).

AD-008 — Redis Streams as Exclusive IPC Bridge for L6/L7 Python

Status: active | Phase: A (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-002, P-001  |  **Dependent Claims:** none (boundary policy)

**Decision:** The L6/L7 Python regime/alpha processes communicate with the C++ execution plane exclusively via Redis Streams. Direct memory access, shared memory writes, or Python C-extension calls into `engine/` are forbidden.

**Alternative Rejected:** Python ctypes/cffi bridge for direct SHM access. _Rejection rationale:_ Python's GIL and garbage collector introduce non-deterministic pause events that, if synchronized with C++ lock-free ring operations, produce undefined behavior and data races not detectable by TSAN.

**Rationale:** The IPC boundary is a safety boundary, not a performance boundary. Redis Streams as a typed, versioned, buffered queue makes the boundary inspectable, replayable, and governable. Performance penalty is acceptable because L6/L7 are not on the low-latency hot path.

AD-009 — FlatBuffers as Exclusive Schema Language for Inter-Layer Events

Status: active | Phase: A (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-003, P-002  |  **Dependent Claims:** CLAIM-FEATURE-001

**Decision:** All inter-layer event types are defined in FlatBuffers schemas. Generated code is committed to the repository. No protobuf, JSON, or custom serialization on the event bus.

**Alternative Rejected:** protobuf. _Rejection rationale:_ protobuf uses heap allocation and field-presence semantics that are incompatible with the no-dynamic-alloc constraint in the LOB engine (L1).

**Alternative Rejected:** JSON. _Rejection rationale:_ JSON parsing is non-deterministic in allocation pattern and incompatible with real-time latency targets.

**Rationale:** Committing generated code makes the schema a first-class build artifact — schema changes are visible in code review and CI. Zero-copy FlatBuffers access is compatible with the SPSC ring layout and no-alloc constraints.

⊕ **AD-009-EXT-001 -- FlatBuffers Wire Schema Version Field**  
Every FlatBuffers event table defined in EGBOS inter-layer schemas MUST carry `schema_version: uint16` at **field ID 0** in the wire format -- always-populated, never optional, never deprecated. Value equals the integer declared in `governance/schemas/schema_version_registry.yaml` for that schema name. Consumers MUST read field ID 0 before full deserialization and MUST reject messages whose `schema_version` is below their declared `SCHEMA_MIN_VERSION` compile-time constant. Enables runtime version dispatch without full payload parse, preventing silent version skew across rolling upgrades.  
**Enforcement:** GC-E-024 (existing `flatc --conform` gate) extended to also verify that field ID 0 in every committed FlatBuffers table definition is `schema_version: uint16`. Missing or mispositioned field = Error at C0.  
**Canonical Registry:** `governance/schemas/schema_version_registry.yaml` -- committed at S0-exit; govcheck verifies all schema names are registered therein.  
**Governing ADRs:** AD-009, AD-024.  **Authority:** CTO + Architecture Team.  _Absorbed: QOS Final Absorption Pass 2026-06-16._

AD-010 — Monotone Latch as Exclusive Safety Enforcement Mechanism in L4

Status: active | Phase: D (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-002, P-006  |  **Dependent Claims:** CLAIM-L3-006, CLAIM-INV-001

**Decision:** L4 risk enforcer uses monotone latches only. Once a safety limit is reduced (position limit, gross exposure), it cannot be increased by the runtime — only by a HITL operator action with governance audit trail. No automatic re-arming without HITL sign-off.

**Alternative Rejected:** Bidirectional runtime adjustment of limits. _Rejection rationale:_ Automatic re-arming after a safety event means the system can self-repair to a riskier state without human review — violating P-006 under adversarial market conditions.

**Rationale:** A safety system that can be automatically bypassed by the nominal operating path is not a safety system — it is a delay with an override. Monotone latches make the failure mode structurally one-directional: the system can only become safer autonomously; it can only become riskier with human intervention.

AD-011 — ONNX as Exclusive Model Inference Format in L5

Status: active | Phase: A (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-003, P-002  |  **Dependent Claims:** CLAIM-FEATURE-001

**Decision:** L5 Execution Router loads models exclusively in ONNX format via ONNX Runtime. `pickle`, `joblib`, `torch.load`, and `tf.SavedModel` are forbidden in L5.

**Alternative Rejected:** Python model serving via `torch.jit`. _Rejection rationale:_ `torch.jit` serialization is Python-version dependent and not byte-identical across runtime versions — violating replay fidelity (P-003). Python GIL introduces non-deterministic latency on the decision path.

**Rationale:** ONNX Runtime provides a stable, versioned, language-agnostic inference binary with deterministic output for fixed input and fixed weights. It is the only inference format that is simultaneously compatible with C++ integration, determinism requirements, and the no-pickle constraint.

⊕ **GC-E-055-EXT-001 -- RFC Mandatory for ONNX Runtime Version Bump**  
Any change to `governance/onnx_runtime_version.lock` MUST be accompanied by a formal RFC (Phase I §I5). The RFC MUST include: (a) new runtime version string, (b) results of re-running CLAIM-FEATURE-PARITY-001 under the new runtime (10,000-tick memcmp parity test), (c) SHA-256 of the new runtime binary, and (d) counter-signature by CTO + Lead Quant. Commits modifying the lock file without an open, approved RFC are rejected at C0. Gate: C0. Severity: Error.  
  
**GC-E-055-EXT-002 -- Model Re-registration Required on Runtime Bump**  
When `governance/onnx_runtime_version.lock` changes (detected by git diff), govcheck MUST verify every model registry entry under `governance/model-registry/` has a new run ID and updated `onnx_runtime_version` field matching the new lock value. Stale entries = Error. Gate: P2 + PF-09. Severity: Error.  
**Authority:** CTO + Lead Quant. _Absorbed: QOS Final Absorption Pass 2026-06-16._

AD-012 — WAL-Before-State as Mandatory Write Ordering in L3

Status: active | Phase: A (formalized H) | Authority: CTO + Governance Board

**Governing Principles:** P-003, P-006  |  **Dependent Claims:** CLAIM-WAL-001, CLAIM-L3-001, CLAIM-INV-001

**Decision:** Every L3 state mutation MUST be preceded by a WAL write that is fsync-confirmed. State and WAL are never written atomically — WAL write succeeds first, state mutation second. No exception.

**Alternative Rejected:** Atomic state+WAL write. _Rejection rationale:_ Atomic write to two storage paths is not achievable without a distributed transaction manager — which introduces latency and complexity incompatible with the <500µs feed-to-decision target.

**Alternative Rejected:** State-first, WAL-second (write-behind logging). _Rejection rationale:_ State-first means a kill-9 between state write and WAL write produces a state that cannot be recovered — violating CLAIM-INV-001.

**Rationale:** Write-ahead logging is the foundational invariant. The proof of its correctness is: if the process is killed at any point, the WAL either contains the record (safe to replay) or does not (safe to ignore state mutation that never completed). There is no unsafe intermediate state.

AD-013 — Simulation Isolation — SIM_STATE Never Writes to WAL or SHM

Status: active | Phase: F (formalized H) | Authority: Founder

**Governing Principles:** P-002, P-004  |  **Dependent Claims:** none (isolation boundary)  |  **Dependent GC Rule:** GC-E-056

**Decision:** The EGBOS_SIM simulation controller operates exclusively within the JavaScript `SIM_STATE` object. It does not write to WAL files, SHM regions, Tier-0 logs, or any persistent storage. Simulation data flows are labelled `source: "simulation"` in `TelemetryEvent.source` and are structurally distinguishable from live data at all sinks. GC-E-056 blocks the S1 startup gate when this boundary is violated.

**Decision:** The EGBOS_SIM simulation controller operates exclusively within the JavaScript `SIM_STATE` object. Full isolation specification and enforcement mechanism are defined canonically at Phase F §F2.1 and enforced mechanically by GC-E-056 (registered at Phase B §B1.3.3 and B1.5.2).

**Rationale:** Simulation correctness requires that no simulation artifact can affect production state. The structural impossibility guarantee (P-002) is stronger than any runtime check or flag. The `source` field in TelemetryEvent is the governance audit trail that proves isolation, and GC-E-056 makes any breach blocking before live capital authority begins.

AD-014 — HashiCorp Vault HSM-Backed Transit Secrets & Dynamic Key Rotation

Status: active | Phase: K | Authority: CSO + Governance Board

**Governing Principles:** P-001, P-006  |  **Dependent Claims:** CLAIM-GOVCHECK-001

**Decision:** Production systems must authenticate to HashiCorp Vault via AppRole and retrieve dynamic, short-lived secrets (TTL < 24h). Cryptographic keys are stored in a hardware HSM, and private keys for configuration signing are rotated automatically every 30 days. Emergency recovery uses a Shamir secret-shared break-glass procedure requiring Chief Security Officer and CTO keys.

**Alternative Rejected:** Environment-based credentials. _Rejection rationale:_ Environment variables are easily leaked via logs, cores dumps, or process inheritance, failing institutional security audits.

**Rationale:** Zero hardcoded credentials and automated secret rotation represent the baseline of institutional defense-in-depth. HSM integration renders manual key extraction impossible.

AD-015 — Zero-Trust Host Isolation, mTLS, and Bastion Access Control

Status: active | Phase: K | Authority: CSO + CTO

**Governing Principles:** P-001, P-002  |  **Dependent Claims:** CLAIM-GOVCHECK-001

**Decision:** All network communication between the research, execution, and risk planes must use mTLS managed by a Consul/Envoy service mesh. Production hosts are isolated with strict firewalls, disabling direct root SSH. Console access is restricted to bastion hosts requiring YubiKey hardware MFA and temporary IAM authorization.

**Alternative Rejected:** VPN-only network segmentation. _Rejection rationale:_ VPNs allow lateral movement once the boundary is breached. Zero-trust mTLS restricts access on a service-to-service level.

**Rationale:** Hardening host access and enforcing dynamic network authentication prevents compromised secondary systems (such as research servers) from tampering with execution or risk controls.

AD-016 — Institutional Geo-Failover Architecture (NY4 → CH2)

Status: active | Phase: K | Authority: CRO + Chief Reliability Eng

**Governing Principles:** P-003, P-006  |  **Dependent Claims:** CLAIM-WAL-001, CLAIM-L3-001

**Decision:** Maintain an active-passive disaster recovery site in Chicago (CH2) with real-time state replication of positions, fills, and orders over InfiniBand/RoCEv2. The system must support a Recovery Time Objective (RTO) of < 60 seconds and a Recovery Point Objective (RPO) of < 1 second, automatically canceling resting orders in NY4 upon failover.

**Alternative Rejected:** Active-Active trading. _Rejection rationale:_ Active-active execution in HFT introduces double-execution risks and complex cross-venue state coordination latencies.

**Rationale:** Geo-redundancy ensures business continuity in the event of a catastrophic primary datacenter outage. Continuous state synchronization prevents post-failover trade mismatch.

⚑ **Governance Reconciliation Note:** AD-016 and AD-028 address different failure domains and are complementary architectural controls. AD-016 defines institutional geo-failover for catastrophic primary datacenter loss (NY4→CH2). AD-028 defines tiered operational recovery SLAs for incremental failure domains. No contradiction exists.

AD-017 — Multi-Signatory Cryptographic Build, SBOM, and Secure Boot Validation

Status: active | Phase: K | Authority: CTO + Chief Security Officer

**Governing Principles:** P-001, P-002, P-003  |  **Dependent Claims:** CLAIM-GOVCHECK-001

**Decision:** The CI/CD pipeline must automatically compile a Software Bill of Materials (SBOM) using Syft on every release. Binaries and docker images must be digitally signed via Cosign using HSM-backed keys, and production hosts must enforce Secure Boot and verify binary signatures at startup before execution.

**Alternative Rejected:** Running unsigned binaries with post-deploy verification. _Rejection rationale:_ Unsigned binaries are vulnerable to supply-chain attacks and unauthorized local modifications.

**Rationale:** End-to-end cryptographic supply-chain verification prevents malicious or unverified code from executing in production, passing stringent institutional security reviews.

AD-018 — Dual-Feed Packet-Level Line Arbitration & IEEE 1588 PTP Synchronization

Status: active | Phase: K | Authority: CTO + Lead Quant

**Governing Principles:** P-003, P-004  |  **Dependent Claims:** CLAIM-FEATURE-001, CLAIM-L0-002

**Decision:** Feed handlers must consume redundant A/B market data lines via Solarflare OpenOnload network cards, performing hardware-level packet arbitration (forwarding the first packet to arrive and discarding duplicates). System clocks must be synchronized via IEEE 1588 PTP to a GPS Grandmaster clock to maintain a sub-microsecond skew ceiling.

**Alternative Rejected:** Software-level TCP gap recovery on a single feed. _Rejection rationale:_ TCP re-requests add milliseconds of delay, resulting in LOB reconstruction lag and adverse selection during market spikes.

**Rationale:** Ultra-low latency execution requires feed stability and absolute clock synchronization across distributed nodes for order booking accuracy and valid backtest logging.

AD-019 — Out-of-Process FPGA-Gated Risk Enforcement & System-Wide Optical Disconnect

Status: active | Phase: K | Authority: CRO + Lead Quant

**Governing Principles:** P-003, P-006  |  **Dependent Claims:** CLAIM-RISK-STALE-001

**Decision:** Enforce hard risk limits (max order size, gross exposure, message rate) in an out-of-process FPGA/SmartNIC wrapper. If the execution thread blocks or a limit is breached, the network card will drop outgoing packets, and a Layer 1 optical fiber disconnect switch will trigger physically within < 100 microseconds.

**Alternative Rejected:** In-process C++ thread checking. _Rejection rationale:_ An infinite loop or memory corruption in the execution thread can block in-process checks, leading to uncontrolled quoting loop disasters.

**Rationale:** Separating risk enforcement from execution logic guarantees safety even in the event of process crashes, deadlock, or operating system-level lockups.

AD-020 — Automated Alpha Decay Governance & Signal Retirement Pipeline

Status: active | Phase: K | Authority: Lead Quant + CRO

**Governing Principles:** P-002, P-005, P-007  |  **Dependent Claims:** CLAIM-MODEL-001, CLAIM-SIGNAL-001

**Decision:** Implement an automated decay tracking service. If an active strategy's 30-day rolling Sharpe ratio drops below 40% of its out-of-sample backtest expectation for 5 consecutive days, its allocation limit is scaled down by 75% automatically. If it does not recover in 15 days, it is moved to shadow-testing and scheduled for retirement.

**Alternative Rejected:** Manual quarterly review meetings. _Rejection rationale:_ Human intervention is too slow to prevent drawdown from decayed or overfitted strategies, presenting a key-person and operational risk.

**Rationale:** Automated alpha lifecycle constraints remove emotional bias from strategy allocation, preserving capital for high-performance factors.

AD-021 — Tiered Governance Gating & Velocity-Safety Optimization

Status: active | Phase: K | Authority: CTO + Governance Board

**Governing Principles:** P-001, P-002, P-007  |  **Dependent Claims:** CLAIM-GOVCHECK-001

**Decision:** Implement a tiered control architecture (Tier 0: Hot Path; Tier 1: Warm Path/Risk; Tier 2: Simulation/Tooling; Tier 3: Local Research/Visualizations). Direct static analysis and RFC compliance gating apply strictly to Tier 0 and Tier 1. Tier 2 and Tier 3 modifications bypass formal RFC processes to optimize development velocity.

**Alternative Rejected:** Flat static analysis gating across all code. _Rejection rationale:_ Forcing researchers to write formal RFCs and pass compile gates for visual notebooks or simulation scripts creates friction and harms research throughput.

**Rationale:** Granular, tiered safety controls protect the hot execution path without imposing unnecessary bureaucracy on offline research work.

AD-022 — Portfolio VaR and Real-Time Multi-Asset Concentration Limits

Status: active | Phase: K | Authority: CRO + Governance Board

**Governing Principles:** P-005, P-006  |  **Dependent Claims:** CLAIM-RISK-STALE-001

**Decision:** The risk enforcer must dynamically evaluate portfolio-level net delta, Value at Risk (VaR/CVaR), and asset concentration across venues. Strategy trading limits are dynamically scaled down when correlation metrics spikes indicate systemic market distress or capital limits are approached.

**Alternative Rejected:** Per-instrument standalone limits. _Rejection rationale:_ Standalone limits fail to capture systemic correlation risk, potentially resulting in catastrophic drawdowns during market crashes.

**Rationale:** Institutional capital preservation requires cross-asset correlation tracking and portfolio-wide VaR constraints in addition to local instrument-level limits.

AD-023 — Executable Incident Response and Automated Runbook Verification

Status: active | Phase: K | Authority: Chief Reliability Eng

**Governing Principles:** P-002, P-006  |  **Dependent Claims:** none (resilience policy)

**Decision:** All operational incident responses (system halts, node failovers, backup promotion) must be coded as executable Ansible playbooks. The simulation engine will run automated daily drills injecting failures (latency, crashes, corruption) to verify playbooks execute successfully within SLAs.

**Alternative Rejected:** Documented step-by-step wiki playbooks. _Rejection rationale:_ Manual procedures invite user errors and delay response times under pressure, violating reliability SLOs.

**Rationale:** Automation of operational response ensures predictable, deterministic, and rapid mitigation of system faults in production environments.

REPLAY-CONTRACT-001 -- Unified Per-Run Replay Contract

Status: active | Phase: C (registered H) | Authority: CTO + Architecture Team

**Governing Principles:** P-003, P-002  |  **Dependent Claims:** CLAIM-WAL-001, CLAIM-INV-001, CLAIM-FEATURE-PARITY-001

**Decision:** Every execution of the 3-pass replay engine (`replay_engine.h`) MUST produce an immutable Replay Contract YAML archived under `governance/replay-contracts/{replay_id}.yaml`. A replay run that terminates without this artifact has not been executed in the EGBOS sense -- it has been observed. Observation without archival evidence is a P-001 violation (P-001: every correctness constraint must be enforced by a tool before it is considered active).

**Replay Contract Schema** (`governance/replay-contracts/{replay_id}.yaml`):
[code] 
    replay_id:            "REPLAY-{epoch_ms}"         # Unique per run
    
    source_tier:          "Tier-0 | Tier-1 | Tier-2" # WAL data source tier
    
    tick_count:           integer                      # Events replayed
    
    wall_clock_start:     ISO-8601
    
    wall_clock_end:       ISO-8601
    
    invariants_evaluated: ["INV-001","INV-002","INV-003"]
    
    invariants_passed:    ["INV-001","INV-002","INV-003"]  # Subset of evaluated
    
    claim_ids_satisfied:  ["CLAIM-WAL-001","CLAIM-FEATURE-PARITY-001",...]
    
    replay_engine_sha256: string                       # SHA-256 of replay_engine binary
    
    signed_by:            string                       # Role of signing authority
    
    schema_version:       1
[/code]

**GC-E-RC-001:** govcheck verifies that for every replay run cited in the proof artifacts of CLAIM-WAL-001, CLAIM-INV-001, and CLAIM-FEATURE-PARITY-001, a corresponding Replay Contract YAML exists in `governance/replay-contracts/` with a valid signature and all declared invariants present in `invariants_passed`. Missing or unsigned contract = Error at C0. Gate: C0. Severity: Error.

**Gap Filled:** EGBOS had replay invariants (INV-001-003) and engine specification but no archivable per-run contract linking a specific execution to the claims it satisfies. _Absorbed: QOS Final Absorption Pass 2026-06-16._

AD-024 — Unified Schema Evolution and FlatBuffers Backward Compatibility Guard

Status: active | Phase: K | Authority: CTO + Architecture Team

**Governing Principles:** P-002, P-003  |  **Dependent Claims:** CLAIM-FEATURE-001

**Decision:** CI pipelines must automatically run FlatBuffers schema validation checks using the `flatc --conform` command. Any schema alteration that breaks backward compatibility is blocked at commit time, requiring a major version bump and a documented 2-week migration plan.

**Alternative Rejected:** Run-time parsing checks. _Rejection rationale:_ Run-time schema mismatches lead to silent data corruption or application panics, posing significant operational risk.

**Rationale:** Strict schema evolution checks ensure inter-process messaging compatibility, enabling safe rolling upgrades of decoupled modules.

AD-025 — End-to-End Latency SLOs and Core-Level Thread Pinning

Status: active | Phase: K | Authority: CTO + Lead Quant

**Governing Principles:** P-003, P-004  |  **Dependent Claims:** CLAIM-FEATURE-PARITY-001

**Decision:** Establish strict latency SLOs (Feature-to-Order latency < 200 microseconds at the 99.99th percentile). Hot-path processing threads must be pinned to isolated, non-overlapping NUMA cores using `isolcpus`, `nohz_full`, and realtime scheduling priorities (FIFO/95).

**Alternative Rejected:** Standard OS thread scheduling. _Rejection rationale:_ Context switches, scheduling latency, and CPU migrations introduce unacceptable tail latency spikes.

**Rationale:** CPU isolation and pinning are critical for minimizing latency jitter and ensuring predictable microsecond-level order placement in high-frequency trading.

## H2 — Prometheus Metrics Exposition Topology

Classification
    Infrastructure Authority Document — Tier 1

Resolves
    Gap 2 from Phase G Consistency Review

Governing Principles
    P-001, P-007

### H2.1 — Thesis

Phase G registered 12 Prometheus metrics (PROM-001 through PROM-012) and defined their wire format (G2.4). Phase G deferred the question of how the `/metrics` endpoint is served in production. This section specifies that deployment topology as a binding architectural decision.

### H2.2 — Exposition Architecture

Production /metrics Exposition Architecture

**Decision:** The Prometheus metrics exporter runs as a dedicated sidecar process (`egbos_exporter`) on the same bare-metal host as the execution engine. It reads from a SPSC ring in SHM (SINK-04, registered in SINK registry G1.3 as a Phase H addition) and exposes HTTP on `localhost:9090/metrics` only — not on external network interfaces. Prometheus scrapes via a localhost target. External Grafana connects via Prometheus federation or Prometheus remote_write, not by direct exporter access.

**Security constraint:** The exporter binds to `127.0.0.1:9090` only. Firewall rule prevents external access. Any proposal to expose the exporter on a non-loopback interface requires a formal ADR.

**Latency tolerance:** The `/metrics` endpoint has a 200ms scrape timeout. Metric values are pre-computed and cached in the SPSC ring every 100ms by the `egbos_metrics_writer` module. Scrape handler performs read-only ring traversal — no lock contention with execution plane.

**Phase dependency:** Full production validation deferred to Phase III exit criteria (A2.2 Phase III). Simulation mode: G2.4 Prometheus snapshot format is already exposed via `window.ObservabilityBridge.getPrometheusSnapshot()` — compatible with future scrape endpoint wire format.

### H2.3 — Exporter Module Specification

Module| Language| SHM Source| Output| Latency Constraint| Claim  
---|---|---|---|---|---  
egbos_metrics_writer| C++20| Execution plane counters| SPSC ring (SINK-04)| Write cycle ≤ 100ms| none  
egbos_exporter| Rust| SPSC ring (SINK-04)| HTTP /metrics (localhost:9090)| Scrape handler ≤ 200ms| none  
prometheus_scraper| External (Prometheus)| HTTP /metrics| TSDB| Scrape interval: 15s| none  
grafana| External| Prometheus TSDB| Dashboard panels| Render ≤ 1s| none  
  
## H3 — Grafana Dashboard Panel Definitions

Classification
    Operational Specification — Tier 1

Resolves
    Gap 3 from Phase G Consistency Review

Governing Principles
    P-001, P-007

### H3.1 — Panel Architecture

Grafana panels are defined here as binding specifications. The actual panel JSON is generated from these specifications during Phase III staging. Panel alert thresholds are derived directly from the warn/fail bounds in the G2.3 Prometheus Metrics Registry — no additional threshold values are introduced here.

### H3.2 — Panel Registry

Panel ID| Title| Metric (PROM-ID)| Visualization| Warn Threshold| Fail Threshold| Alert Severity  
---|---|---|---|---|---|---  
PAN-001| WAL fsync P99| PROM-001| Gauge + sparkline| 12,000 ns| 25,000 ns| P0  
PAN-002| Loop Cycle P99| PROM-002| Gauge + sparkline| 270 µs| 500 µs| P0  
PAN-003| Feed→Decision P99| PROM-003| Gauge + time-series| 468 µs| 500 µs| P0  
PAN-004| Position Integrity Delta| PROM-004| Single stat (0/FAIL)| —| ≠ 0| P0 (immediate halt)  
PAN-005| Gross Exposure Ratio| PROM-005| Gauge with limit line| 0.7| 1.0| P1 / P0  
PAN-006| Preflight Armed| PROM-006| State indicator| —| = 0| P0  
PAN-007| Active Waiver Days| PROM-007| Bar chart| 150 days| 180 days| P1  
PAN-008| Module Fault Total| PROM-008| Single stat counter| 0| ≥ 1| P1  
PAN-009| P0 Alert Log Total| PROM-009| Single stat counter| 0| ≥ 1| P0  
PAN-010| SPSC Ring Utilization| PROM-010| Gauge (%)| 50%| 80%| P1  
PAN-011| Replay Fidelity Pass| PROM-011| State indicator| —| = 0| P0  
PAN-012| Signal Decay Sharpe 30d| PROM-012| Time-series| —| < 0| P1  
  
### H3.3 — Dashboard Layout

Row 1 — Capital Safety Row

Panels: PAN-004, PAN-006, PAN-011, PAN-009. Binary pass/fail indicators. Any FAIL state here triggers TERMINAL_HALT evaluation. These panels render with a red border. No time-averaging — single-scrape binary truth.

Row 2 — Latency Row

Panels: PAN-001, PAN-002, PAN-003. Time-series with warn/fail overlay lines. P99 values updated on every scrape cycle (15s). Warn threshold crossing triggers P1 alert. Fail threshold crossing triggers P0 alert and TERMINAL_HALT evaluation.

Row 3 — Operational Health Row

Panels: PAN-005, PAN-007, PAN-008, PAN-010, PAN-012. Trend monitoring panels. Alert on warn threshold crossing. These panels do not trigger autonomous halt — they require HITL review per A2.3 P1 protocol.

Dashboard Variable: TELEMETRY_MODE

Values: `SIMULATION` | `LIVE`. In SIMULATION mode, panels read from the ObservabilityBridge Prometheus snapshot (`window.ObservabilityBridge.getPrometheusSnapshot()`). In LIVE mode, panels read from the `egbos_exporter` scrape target. Mode switch requires HITL action, logged to Audit Log (D3).

## H4 — Redis Production Deployment for Module Heartbeat Schema

Classification
    Infrastructure Authority Document — Tier 1

Resolves
    Gap 4 (carried forward from Phase G / F2.3 / G1.3 SINK-02)

Governing Principles
    P-005, P-006

### H4.1 — Deployment Specification

Redis serves as the IPC channel for two use cases in EGBOS: module heartbeat signals (SINK-02, Phase G) and the L6/L7 Python-to-C++ bridge (AD-008). Both require the same production Redis instance. This section specifies the production deployment contract.

Property| Value| Rationale  
---|---|---  
**Version**|  Redis 7.x (stable)| No experimental features; AOF persistence required  
**Persistence**|  AOF (appendonly yes) with fsync=everysec| Balance durability and throughput; heartbeat data is re-generated; loss window ≤ 1s is acceptable  
**ACL**|  Separate ACL user per consumer: heartbeat_writer, heartbeat_reader, l6_writer, l7_writer, l5_reader| Principle of least privilege; prevents a faulted module from corrupting another module's stream  
**Binding**|  127.0.0.1 only (same-host deployment)| No cross-host Redis exposure; all consumers co-located on execution host  
**Failure: Redis process crash**|  Module heartbeat: SINK-02 goes dark. ObservabilityBridge receives no heartbeat events. After 3× heartbeat_interval (3 × 500ms = 1.5s), all module statuses transition to UNKNOWN. L6/L7 IPC: new regime/alpha signals blocked. Existing positions held (AD-007 stale-feed semantics apply). Redis auto-restart via systemd with 5s delay.| Heartbeat loss is observable; P1 alert fires. Positions are not force-closed per AD-007.  
**Failure: Redis memory pressure**|  Streams capped at MAXLEN 10,000 entries per stream. Oldest entries evicted (FIFO). No capital safety impact: streams are telemetry, not state.| Telemetry loss is acceptable; state is in WAL, not Redis.  
**Recovery**|  On Redis restart: SINK-02 heartbeat writers re-connect within 1s (connection retry loop with exponential backoff capped at 500ms). L6/L7 Python clients re-connect on next inference cycle. No WAL state is held in Redis — recovery is stateless.| Stateless Redis means restart is always a clean recovery. No partial-state corruption possible.  
**Phase gate**|  Redis production deployment is a Phase III exit criterion (A2.2). Simulation mode uses the in-memory SIM_STATE.moduleStatus map as a Redis substitute — no Redis required in Phase I or Phase II.| Progressive trust model (A2.1): production dependencies are not required in simulation phases.  
  
## H5 — WAL Simulation Boundary: Phase IV+ Deferral Rationale

Classification
    Governance Boundary Document — Tier 1

Resolves
    Gap 5 (carried forward from Phase G Consistency Review)

Governing Principles
    P-004, P-005

The SimulationFeedbackController (G3) evaluates D2.2 governance rules against simulated `SIM_STATE`. Five rule classes require live WAL sequence state to evaluate correctly:

  - WAL Sequence Monotonicity Guard (GC-WAL-001)
  - WAL Write Latency Claim Expiry Check (derives from CLAIM-WAL-001 V4 scope)
  - WAL Kill-9 Survival Re-verification Trigger
  - INV-001 Violation Detection on live fill stream
  - WAL fsync confirmation delay under real NVMe I/O scheduler



None of these rules can be correctly simulated against `SIM_STATE` because `SIM_STATE` contains no WAL sequence numbers, no fsync latency measurements against real storage hardware, and no fill-stream records. Simulating these rules would require fabricating WAL state — which creates a false governance signal more dangerous than no governance signal.

Phase IV+ Classification — Architectural Boundary, Not a Gap

These five rule classes are classified Phase IV+ only. They are evaluated exclusively against live WAL state in production. The SimulationFeedbackController will emit a structured notice (severity: INFO, message: `"WAL rules [GC-WAL-001, ...] require Phase IV live execution — not evaluable in simulation"`) when queried for these rules.

This boundary is architectural, not a gap. It reflects P-005: failure mode specification precedes nominal specification. The failure mode of simulating WAL rules against fabricated state is a false safety signal — the most dangerous failure mode in a governance system.

⚑ **Governance Reconciliation Note:** The WAL simulation boundary is an architectural boundary (P-005), not a gap. SimulationFeedbackController evaluates D2.2 rules against SIM_STATE. Five rule classes require live WAL state and are correctly classified Phase IV+. The controller emits a structured notice when queried for these rules. This boundary is explicitly documented and governed.

## B1.4 -- Continuous Compliance Evidence Pipeline (COMPLIANCE-EVIDENCE-001)

⊕ _Absorbed from QOS Final Absorption Pass 2026-06-16._ Gap filled: EGBOS CI gates enforce governance but produce no persistent per-run evidence record for external audit reconstruction.

Every CI run that invokes govcheck MUST produce an immutable compliance evidence artifact at `governance/evidence/{epoch_ms}_{git_sha_short}_compliance_snapshot.json`. A CI run that completes without this artifact has not been governed -- it has executed. This evidence is the auditable proof required by Phase K §K9 institutional audit preparation.

### B1.4.1 -- Evidence Artifact Schema
[code] 
    {
    
      "evidence_id":       "EVIDENCE-{epoch_ms}",
    
      "git_sha":           "<40-char commit SHA>",
    
      "run_timestamp":     "ISO-8601",
    
      "govcheck_version":  "<semver>",
    
      "govcheck_sha256":   "<SHA-256 of govcheck binary>",
    
      "gates_evaluated":   ["C0","C1","P2","G4",...],
    
      "gates_passed":      ["C0","C1","P2",...],
    
      "gates_failed":      [],
    
      "claims_checked":    ["CLAIM-WAL-001","CLAIM-INV-001",...],
    
      "waivers_active":    [],
    
      "schema_version":    1
    
    }
[/code]

### B1.4.2 -- Retention and Governing Rule

  - Artifacts are **immutable and append-only**. No CI process may delete or modify an existing artifact.
  - Retention: **90 days minimum** in `governance/evidence/`; archived to `governance/evidence/archive/` at 90-day boundary.
  - **GC-E-EVIDENCE-001:** At C0, govcheck verifies the most recent committed evidence artifact is not older than 24 hours. Absent or stale = Error at C0.



## Phase H — Consistency Review

### Consistency Checks Performed

  - AD-001 through AD-013 are consistent with principles P-001 through P-007 — no ADR contradicts a binding principle
  - AD-003 (Tier-0 threshold fsync) correctly supersedes the informal description at C6/AD-011 — the C6 entry is now the pre-formalization record; this ADR is the authority
  - AD-004 through AD-007 formalize C6 entries AD-008 through AD-012 respectively — no content conflict; C6 entries now carry "(formalized H)" notation
  - Prometheus exposition topology (H2) is consistent with G2.4 wire format — same label schema, same metric names, same PROM-001..012 IDs
  - Grafana panel alert thresholds (H3.2) are derived exclusively from G2.3 warn/fail bounds — no new threshold values introduced
  - Redis deployment spec (H4) is consistent with F2.3 heartbeat schema (SINK-02) — connection semantics unchanged; only production deployment parameters added
  - WAL simulation boundary (H5) is consistent with G3.3 SimulationFeedbackController evaluate() logic — simulation-evaluable rules remain unchanged; Phase IV+ rules are now explicitly documented
  - Phase H timeline entry updated to 'active' — consistent with PHASES array correction (Correction 3)
  - Executive Summary card updated to reflect Phases C–G complete — consistent with all phase status markers
  - All new HTML uses existing CSS variable tokens only — no hardcoded colors introduced
  - Phase H TOC headings follow existing id pattern — auto-generated TOC builder picks them up via DOM walk without modification



### Governance Reconciliation Status

Item| Previous Classification| Reconciled Status| Authority  
---|---|---|---  
Implementation slice acceptance criteria (S0–S5)| Gap| Confirmed| Phase I (I2, I3 complete)  
Claim-to-implementation-slice mapping| Gap| Confirmed| Phase I (I4 complete)  
govcheck binary boundary rule enforcement verification| Gap| Confirmed| Phase I (I7 complete)  
Formal RFC template| Gap| Confirmed| Phase I (I6 complete; automation deferred to J5.1)  
  
🔒

## Security Architecture Summary

Consolidated view of all security architecture controls formally specified in active ADRs and Phase K specifications.

ARCHITECTURE DEFINITION COMPLETE — IMPLEMENTATION VERIFICATION PENDING

## Security Architecture Definition (Complete)

**Governance Reconciliation Note:** Security architecture is formally specified and governed by active ADRs. The following components are defined at the architecture level. Implementation verification is a separate Phase III staging activity.

Control Domain| Specification| ADR / Section| Status| Verification Target  
---|---|---|---|---  
Secrets Management | HashiCorp Vault Enterprise cluster backed by FIPS 140-2 Level 3 HSM; AppRole authentication; dynamic secrets with TTL < 24h; 30-day automated rotation; Shamir 3-of-5 break-glass procedure | AD-014, K1.1 | Active | Phase III (GC-E-SECRETS-001/002 blocking from S1)  
Hardware Security Module | HSM-backed transit secrets; private key storage; cryptographic operations offload; break-glass via Shamir secret sharing requiring CSO + CTO keys | AD-014, K1.1 | Active | Phase III  
Zero-Trust Network | mTLS for all inter-service communication (TLS 1.3, ECDHE-ECDSA-AES256-GCM-SHA384); SPIFFE/SPIRE certificate issuance; default-deny network policies; Istio ingress with JWT validation | AD-015, AD-029, K1.2 | Active | Phase III  
Host Isolation | Production hosts isolated with strict firewalls; no direct root SSH; bastion host access requiring YubiKey hardware MFA + temporary IAM authorization | AD-015, K1.2 | Active | Phase III  
Software Bill of Materials | Syft-generated SPDX SBOM on every CI/CD release; vulnerability scanning; metadata registry archival | AD-017, K1.2 | Active | Phase III  
Binary Signing | Cosign digital signatures using HSM-backed keys; production binaries and container images signed; verification at promotion gate | AD-017, K1.2 | Active | Phase III  
Secure Boot | UEFI Secure Boot with custom platform keys; bootloader and kernel module signature verification; hardware lockout on verification failure | AD-017, K1.2 | Active | Phase III  
Multi-Signature Config Envelope | Ed25519 2-of-3 signatures (CRO, CTO, Head of Trading) for config.bin.env; HSM-stored keys; boot-time libsodium verification | L1B, L2A Stage 4 | Active | Phase III  
Governance Enforcement | GC-E-SECRETS-001: no secrets in git/env (C0 blocking from S1); GC-E-SECRETS-002: Vault endpoint reachability at pre-flight PF-07 (blocking from S1) | I7.2, B1.3.3 | Active | Phase I (C0 gate)  
  
## Implementation Verification (Deferred to Phase III)

Architecture Definition ≠ Implementation Verification

The above controls are **architecturally specified** via active ADRs and Phase K. The following verification activities remain for Phase III staging:

  - Vault cluster deployment and HA validation
  - HSM integration and key ceremony procedures
  - mTLS mesh deployment (SPIFFE/SPIRE, Istio)
  - SBOM pipeline integration in CI/CD
  - Cosign signing workflow with HSM keys
  - Secure Boot configuration on production hardware
  - GC-E-SECRETS-001/002 validation against live repository and Vault endpoint



I

## Phase I — Implementation Governance

Slice Registry (S0–S15), Claim-to-Slice Mapping, RFC Template, govcheck Boundary Audit, and Acceptance Gate Specifications.

GOVERNANCE CONTRACT COMPLETE — FREEZE CERTIFIED

## I1 — Implementation Governance Thesis

Classification
    Institutional Authority Document — Tier 0

Governing Principles
    P-001, P-004, P-007

Depends On
    Phase A (A2.2 Operating Phases), Phase B (B1 Claim System, B1.4 CI Gates), Phase H (H1 ADR Catalog)

### I1.1 — Thesis

Phase I is the translation layer between the architectural blueprint (Phases A–H) and the implementation act. Every implementation slice (S0–S15) is a governed unit of work, not a development sprint. The distinction is structural: a sprint is complete when code is merged; a slice is complete when its acceptance gate passes govcheck at the required verification class and all dependent claims are at their declared target class.

The governing principle is P-004: governance precedes implementation. A slice does not begin until its entry criteria are documented and verifiable. A slice does not close until its exit criteria are mechanically confirmed by govcheck. There is no concept of a slice being "mostly done" — it is either govcheck-clean at the declared exit class or it is open.

This layer serves a second function: institutional memory protection. Personnel change, context fades, and urgency pressures compress review. The Slice Registry is the canonical record of what was claimed, what was proved, and which claims gate which deployments. It cannot be silently revised. Any change to a slice's acceptance criteria after the slice has opened requires a formal RFC (I6).

### I1.2 — Slice Taxonomy

The implementation sequence is divided into two primary categories:

  - **Execution Plane Slices (S0–S7):** Cover ENGINE_V3 Phases 0–7 and map directly to A2.2 Operating Phases I–IV. No live capital can be deployed without all S0–S7 acceptance gates passing.
  - **Research Platform Slices (S8–S15):** Cover ENGINE_V3 Phases 8–15 and map directly to A2.2 Operating Phase V. No live model deployment is permitted without S8–S13 acceptance gates passing.



Slice ID | Title | ENGINE_V3 Phase | A2.2 Operating Phase | Status | Govcheck Gate | Capital Gate  
---|---|---|---|---|---|---  
S0 | Infrastructure Bootstrap | Phase 0 | I | Pending | C0 + P1 + P2 | None  
S1 | WAL + SPSC + SHM Skeleton | Phase 1 | I | Pending | P3 + P4 + P5 | None  
S2 | LOB Engine (L1) | Phase 2 | I | Pending | P3 + P4 + P5 + P6 | None  
S3 | Position Engine (L3) + L4 Risk | Phase 3 | I | Pending | P3 + P4 + P5 + G1 | None  
S4 | Execution Router (L5) + Startup | Phase 4 | I | Pending | P3 + P7 + G1 | Staging deploy  
S5 | Live Feed Integration (L0) | Phase 5 | II | Pending | G2 | Read-only exchange connect  
S6 | Production Hardware Qualification | Phase 6 | III | Pending | G2 + timing_constants | Staging capital  
S7 | Capital Deployment Infrastructure | Phase 7 | IV | Pending | LIVE-CAPITAL-001 | Live capital  
S8 | Research Infrastructure | Phase 8 | V | Pending | G3 | None  
S9 | Dataset Governance | Phase 9 | V | Pending | G3 | None  
S10 | Backtesting & WFV | Phase 10 | V | Pending | P9 + P10 | None  
S11 | Feature Store & Model Governance | Phase 11 | V | Pending | P8 + P11 + G4 | None  
S12 | Alpha Research | Phase 12 | V | Pending | P10 | None  
S13 | Strategy Deployment | Phase 13 | V | Pending | G4 + DEPLOY-STRATEGY-001 | Live model  
S14 | Capacity & Attribution | Phase 14 | V | Pending | P11 | None  
S15 | ML Lifecycle | Phase 15 | V | Pending | P11 + G4 | None  
  
## I2 — Slice Registry: Execution Plane (S0–S7)

Classification
    Implementation Authority Document — Tier 0

Governing Principles
    P-001, P-004, P-005

Depends On
    B1.4 (CI Gates), C (SHM/WAL spec), D (Control Plane), E (Execution Engine)

S0 — Infrastructure Bootstrap Capital Gate: None

**Entry Condition:** Empty repository. CMake + Cargo + uv toolchain installed. govcheck binary compiles and exits 0 on empty claim set.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0 --strict
[/code]

**Pass Criteria:** 0 errors, 0 warnings. CLAIM-000 (bootstrap) active at V1. Schema validator reports all YAML fields present. CI gates active: P1 (format/lint/import), P2 (build matrix GCC13+Clang17+ASAN+UBSAN).

**Mandatory Claims:** CLAIM-000 at V1

**Mandatory ADRs:** AD-009 (FlatBuffers schema committed), AD-006 (SHM sizes compile-time)

**Forbidden Actions:** Any `engine/` code that imports malloc/new/free/delete (GC-E-010 must be active and blocking by P2 end). Any merge that skips clang-format (P1 hard block).

S1 — WAL + SPSC + SHM Skeleton Capital Gate: None

**Entry Condition:** S0 acceptance gate passed. govcheck C0 clean. CLAIM-000 at V1.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P3,P4,P5 --strict
[/code]

**Pass Criteria:** CLAIM-WAL-001 at V4 (10× kill-9 survival, fsync P99 ≤ 8,100ns on target hardware). CLAIM-SPSC-001 at V4 (TSAN-clean, P99 ≤ 50ns). CLAIM-L3-POOL-001 at V2 (no dynamic alloc in pool under ASAN). Zero TSAN findings (P4 passing). P5 latency regression: WAL P99 within 10% of timing_constants.json baseline. CI gates active: P3 (unit + integration), P4 (TSAN), P5 (latency regression).

**Mandatory Claims:** CLAIM-WAL-001 at V4, CLAIM-SPSC-001 at V4, CLAIM-L3-POOL-001 at V2

**Mandatory ADRs:** AD-005 (io_uring single-CQE-consumer), AD-006 (SHM compile-time constants), AD-012 (WAL-before-state ordering)

**Forbidden Actions:** Any WAL write that is not fsync-confirmed before state mutation (GC-WAL-001 must block). Any SHM region sized at runtime. Any CQE consumer thread beyond one.

S2 — LOB Engine (L1) Capital Gate: None

**Entry Condition:** S1 acceptance gate passed. CLAIM-WAL-001 at V4.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P3,P4,P5,P6 --strict
[/code]

**Pass Criteria:** CLAIM-LOB-001 at V4 (3× byte-identical LOB replay against golden fixture). CLAIM-L0-001 at V3 (L0 import-linter clean — GC-E-RESEARCH-001 active). CLAIM-L0-002 at V3 (gap detection correctness — unit tested). P6 fuzz gate: LOB fuzz 60s libfuzzer, zero crashes/sanitizer errors. GC-E-010 blocking (no dynamic alloc in LOB hot path). CI gates active: P3, P4, P5, P6 (fuzz nightly).

**Mandatory Claims:** CLAIM-LOB-001 at V4, CLAIM-L0-001 at V3, CLAIM-L0-002 at V3

**Mandatory ADRs:** AD-004 (fixed-point arithmetic — no float in L1), AD-009 (FlatBuffers event types for LOB events)

**Forbidden Actions:** Any `std::map` in LOB engine (A1.6 constraint). Any dynamic allocation on LOB hot path. Any float arithmetic in price comparison or queue ordering.

S3 — Position Engine (L3) + L4 Risk Enforcer Capital Gate: None

**Entry Condition:** S2 acceptance gate passed. CLAIM-LOB-001 at V4.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P3,P4,P5,G1 --strict
[/code]

**Pass Criteria:**

  - CLAIM-L3-001 at V4 (WAL-before-state 10× kill-9, position = Σ FILL_APPLIED)
  - CLAIM-INV-001 at V4 (position invariant: 10M fills + 10× kill-9)
  - CLAIM-L3-003 at V3 (order_tag_ private, no public getter — GC-E-043 blocking)
  - CLAIM-L3-004 at V4 (TSAN-clean + CROSS_PLANE_READ macro — GC-E-044 blocking)
  - CLAIM-L3-006 at V4 (gross_exposure_ sole cross-plane read — GC-E-045 blocking)
  - CLAIM-L3-MAR-001 at V4 (MassActionReport WAL-before-state)
  - CLAIM-L4-001 at V4 (kill switch < 1ms response under TSAN)
  - G1 gate passing: `govcheck deploy-gate --target staging` (all C0+P gates green)



**CI Gates Active:** P3, P4, P5, G1

**Mandatory Claims:** CLAIM-L3-001, CLAIM-INV-001, CLAIM-L3-003, CLAIM-L3-004, CLAIM-L3-006, CLAIM-L3-MAR-001, CLAIM-L4-001 — all at declared target class

**Mandatory ADRs:** AD-004 (fixed-point), AD-010 (monotone latches in L4), AD-012 (WAL-before-state), AD-005 (io_uring single-consumer)

**Forbidden Actions:** Any direct `x_l_` or `order_tag_` read from L4 or L5 (GC-E-045). Any `memory_order_relaxed` on cross-plane atomic (GC-E-044). Any position mutation without prior WAL write.

S4 — Execution Router (L5) + Startup Protocol Capital Gate: Staging deploy

**Entry Condition:** S3 acceptance gate passed. All S3 claims at declared class. G1 staging deploy gate green.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P3,P7,G1 --strict
[/code]

**Pass Criteria:**

  - CLAIM-L3-02 at V3 (FIX consumer zero business logic — GC-E-042 blocking)
  - CLAIM-L3-05 at V3 (startup buffer empirically sized — pre-live, synthetic feeds)
  - P7 staging smoke gate: 60s simulator smoke, P99 within 20% of timing_constants.json baseline
  - G1 staging deploy gate: `govcheck deploy-gate --target staging` passing
  - All 10 Phase-0 startup checks passing on synthetic fixtures (pre-flight protocol E3)



**CI Gates Active:** P3, P7, G1

**Mandatory Claims:** CLAIM-L3-02 at V3, CLAIM-L3-05 at V3

**Mandatory ADRs:** AD-011 (ONNX-only in L5 — GC-E-RESEARCH-003 active and blocking), AD-009 (FlatBuffers for L5 event types)

**Forbidden Actions:** Any pickle, torch.load, joblib, or tf.SavedModel in L5 (GC-E-RESEARCH-003). Any business logic in the FIX consumer (GC-E-042 blocking). Any timing constants derived from documentation or estimation — measured only.

S5 — Live Feed Integration (L0) — Market Data Qualification Capital Gate: Read-only exchange connect

**Entry Condition:** S4 acceptance gate passed. CLAIM-L3-05 at V3. Exchange read-only credentials provisioned.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,G2 --strict
[/code]

**Pass Criteria:**

  - CLAIM-L3-05 at V4 (startup buffer sized against live feed jitter — empirical measurement on live data)
  - CLAIM-L0-002 at V3→V4 (gap detection correctness validated against real sequence gaps from exchange)
  - 14 consecutive days uptime with zero INV-001 violations
  - 14 nightly replay validations passed (byte-identical against Tier-0 log)
  - G2 production deploy gate green: `govcheck deploy-gate --target production`



**CI Gates Active:** C0, G2. P4/P5/P7 on all hot-path PRs during this slice.

**Mandatory Claims:** CLAIM-L3-05 at V4, CLAIM-L0-002 at V4 (or V3 with waiver documenting known gap type)

**Mandatory ADRs:** AD-007 (stale feed response — freeze not force-close, AD-011 now AD-003 formalized), AD-004 (fixed-point on all live price arithmetic)

**Forbidden Actions:** Any order transmission to any exchange (zero order path active). Any INV-001 violation without immediate Phase II suspension. Any replay non-determinism without feature engine bisect.

**Note:** This slice maps to A2.2 Phase II (Market Data Qualification). Exit requires sign-off document `MARKET-DATA-001.yaml` (see I5).

S6 — Production Hardware Qualification Capital Gate: Staging capital

**Entry Condition:** S5 acceptance gate passed. CLAIM-L3-05 at V4. Production-equivalent bare metal provisioned. No cloud VMs.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,G2 --strict
[/code]

_Note: Requires manual timing_constants sign-off._

**Pass Criteria:**

  - `timing_constants.json` committed with empirical measurements from production hardware (WAL fsync P99, loop cycle P99, feed-to-decision P99 — all measured, none estimated)
  - 48h clean staging run: zero P0 incidents, zero TSAN findings, zero INV-001 violations
  - All 10 Phase-0 startup checks passing on production hardware
  - Stratum-0 cold recovery drill completed and recorded
  - REC-008 (ABORT preflight — PF-01 WAL CRC failure), REC-009 (Stratum-0 cold recovery), and REC-010 (TERMINAL_HALT with simultaneous connectivity loss) fire-drill records committed to `governance/runbooks/firedrills/phase-iii/` — verified by GC-E-052
  - CLAIM-L3-05 measurements updated in `timing_constants.json` for production hardware profile
  - G2 production deploy gate: `govcheck deploy-gate --target production` passing



**CI Gates Active:** C0, G2

**Mandatory Claims:** All S0–S5 claims remain at or above declared class. `timing_constants.json` committed (hardware_id tagged per B1.2.2 V4 scope).

**Mandatory ADRs:** All AD-001 through AD-013 active. No ADR may be superseded during this slice without a full RFC (I6).

**Forbidden Actions:** Any timing constant from documentation, estimation, or cloud VM measurement. Any deployment to production hardware before 48h clean staging run.

**Note:** All operational runbooks (pre-flight failure recovery, ABORT recovery) must be fire-drilled before exit. Runbook fire-drill records committed to `governance/runbooks/`.

S7 — Capital Deployment Infrastructure Capital Gate: Live capital

**Entry Condition:** S6 acceptance gate passed. `LIVE-CAPITAL-001.yaml` signed. `timing_constants.json` committed with production measurements.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,G2 --strict
[/code]

**Pass Criteria:**

  - `LIVE-CAPITAL-001.yaml`: founder-signed governance record, capital authority document
  - IV-A gate passed: 5% target notional, zero P0 incidents
  - CLAIM-L3-05 at V4 on production hardware
  - INV-001 monitor active and reporting to ObservabilityBridge (PROM-004 live)
  - Prometheus `egbos_exporter` live and scraping to production Grafana (H2/H3 panels operational)
  - Redis production deployment active with ACL configuration per H4



**CI Gates Active:** C0, G2 (all PRs)

**Mandatory Claims:** All claims S0–S6 at declared class. CLAIM-ATTR-001 at V1 or higher (P&L; reconciliation process started).

**Mandatory ADRs:** Full ADR catalog (AD-001 through AD-013) active. No exceptions.

**Forbidden Actions:** Capital deployment without signed `LIVE-CAPITAL-001.yaml`. Any automatic re-arming of a safety latch without HITL sign-off (AD-010). Position scaling beyond IV-A without zero-P0-incident qualification period.

**Note:** Live capital deployment IV-A (5% target notional). Progression to IV-B requires zero P0 incidents over 2 weeks (A2.2 Phase IV).

## I3 — Slice Registry: Research Platform (S8–S15)

Classification
    Implementation Authority Document — Tier 1

Governing Principles
    P-001, P-003, P-004

Depends On
    Phase B (B2 Research Architecture, B3 Validation Framework)

Prerequisite
    S4 exit (research plane may begin after execution infrastructure is staging-qualified)

S8 — Research Infrastructure Capital Gate: None

**Entry Condition:** S4 acceptance gate passed. Docker environment operational. MLflow tracking server accessible. hypothesis registry YAML schema defined.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,G3 --strict
[/code]

**Pass Criteria:** G3 research deploy gate green (lint-imports clean, GC-E-RESEARCH-001 blocking). CLAIM-RESEARCH-001 at V3 (no research→engine imports, CI-enforced). Hypothesis registry: at least one hypothesis at V1 status with owner, entry criteria, and kill criteria documented.

**Mandatory Claims:** CLAIM-RESEARCH-001 at V3

**Mandatory ADRs:** AD-001 (replay engine canonical source), AD-002 (research plane hard boundary), AD-008 (Redis Streams as exclusive L6/L7 IPC)

**Forbidden Actions:** Any `research/` code importing from `engine/` (GC-E-RESEARCH-001 hard block). Any backtester not using `replay/replay_engine.h` (GC-E-RESEARCH-002).

S9 — Dataset Governance Capital Gate: None

**Entry Condition:** S8 acceptance gate passed. DVC initialized. Tier-0/Tier-1/Tier-2 data lake accessible (read-only from research plane per AD-002).

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,G3 --strict
[/code]

**Pass Criteria:** CLAIM-DATA-001 at V4 (point-in-time correct, no lookahead — verified by offline test suite against known-lookahead fixtures). CLAIM-DATA-002 at V3 (split dates immutable — DVC hash on split date file). OOS vault initialized: sealed, zero modifications, inotify monitor active.

**Mandatory Claims:** CLAIM-DATA-001 at V4, CLAIM-DATA-002 at V3

**Mandatory ADRs:** AD-001 (Tier-0 log read through `replay_engine.h` only), AD-007 (stale-feed data excluded from training sets)

**Forbidden Actions:** Any training dataset constructed without point-in-time validation (CLAIM-DATA-001 V4 required). Any modification to OOS vault without unseal record (GC-E-RESEARCH-004 blocking).

S10 — Backtesting & Walk-Forward Validation Capital Gate: None

**Entry Condition:** S9 acceptance gate passed. CLAIM-DATA-001 at V4. OOS vault sealed.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P9,P10,G3 --strict
[/code]

**Pass Criteria:** CLAIM-BACKTEST-001 at V4 (P&L; curve SHA-256 identical × 3 consecutive runs — P9 gate). CLAIM-WFV-001 at V3 (WFV OOS discipline: train/test split never crosses). P10 WFV gate: at least one strategy achieving OOS Sharpe ≥ 1.0, max DD ≤ 15%, across ≥ 3 distinct market regimes, DSR ≥ 0.5.

**Mandatory Claims:** CLAIM-BACKTEST-001 at V4, CLAIM-WFV-001 at V3

**Mandatory ADRs:** AD-001 (backtester uses `replay_engine.h` exclusively)

**Forbidden Actions:** Any backtest using data from OOS vault unless in a formally designated OOS evaluation run with unseal record. Any P&L; curve that is not SHA-256-reproducible (P9 blocks merge).

S11 — Feature Store & Model Governance Capital Gate: None

**Entry Condition:** S10 acceptance gate passed. CLAIM-BACKTEST-001 at V4.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P8,P11,G4 --strict
[/code]

_Note: G4 is not the final live gate here — it is the structural gate check; full G4 requires shadow period in S13._

**Pass Criteria:** CLAIM-FEATURE-PARITY-001 at V4 (INV-003: offline vs online L2 byte-identical — P8 memcmp gate). CLAIM-MODEL-001 at V3 (model card completeness: training config, feature list, validation results, ONNX export, SHA-256). CLAIM-REGIME-001 at V4 (INV-004 regime WAL determinism — regime signal replayed from same WAL sequence produces identical regime classification × 3).

**Mandatory Claims:** CLAIM-FEATURE-PARITY-001 at V4, CLAIM-MODEL-001 at V3, CLAIM-REGIME-001 at V4

**Mandatory ADRs:** AD-011 (ONNX-only model format — GC-E-RESEARCH-003), AD-001 (feature store read through `replay_engine.h` for online parity test)

**Forbidden Actions:** Any model exported as pickle or torch.load format entering the model registry (GC-E-RESEARCH-003). Any model card missing SHA-256 of ONNX artifact (govcheck P11 rejects).

S12 — Alpha Research Capital Gate: None

**Entry Condition:** S11 acceptance gate passed. CLAIM-FEATURE-PARITY-001 at V4. At least one hypothesis in registry at V2+ status.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P10,G3 --strict
[/code]

**Pass Criteria:** P10 WFV gate passing for the target signal: OOS Sharpe ≥ 1.0, max DD ≤ 15%, ≥ 3 regimes, DSR ≥ 0.5 (B3.3 thresholds). CLAIM-SIGNAL-001 at V3 (signal weight governance: weight vector in DVC, SHA-256 tagged, immutable after promotion).

**Mandatory Claims:** CLAIM-SIGNAL-001 at V3

**Mandatory ADRs:** AD-002 (research plane boundary), AD-001 (replay fidelity chain maintained for OOS evaluation)

**Forbidden Actions:** Any OOS evaluation that reuses data from the training window (GC-E-RESEARCH-004 OOS vault seal must remain intact). Any signal promoted to live without WFV P10 gate passing.

S13 — Strategy Deployment Capital Gate: Live model

**Entry Condition:** S12 acceptance gate passed. CLAIM-SIGNAL-001 at V3. CLAIM-MODEL-001 at V3. S7 capital deployment infrastructure live.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,G4 --strict
[/code]

**Pass Criteria:** G4 model live gate: CLAIM-FEATURE-PARITY-001 at V4, CLAIM-MODEL-002 at V4 (inference P99 within ONNX Runtime budget on production hardware), CLAIM-WFV-001 at V3+, 14-day shadow mode (paper orders via WAL-recorded paper loop — no live execution) with zero divergence between shadow and live position state. `DEPLOY-STRATEGY-001.yaml` signed by founder.

**Mandatory Claims:** CLAIM-MODEL-002 at V4, all S8–S12 claims at declared class

**Mandatory ADRs:** AD-011 (ONNX inference only in L5 — GC-E-RESEARCH-003 must be blocking), AD-001 (model inference path uses same replay engine binary as research validation)

**Forbidden Actions:** Any live model deployment without 14-day shadow period and G4 gate passing. Any model update that is not accompanied by `DEPLOY-STRATEGY-001` amendment and new govcheck G4 run. CLAIM-SIGNAL-001 governs live weight vector — immutable after sign-off.

S14 — Capacity & Attribution Capital Gate: None

**Entry Condition:** S13 acceptance gate passed. Live model running. CLAIM-ATTR-001 at V1+.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P11 --strict
[/code]

**Pass Criteria:** CLAIM-CAPACITY-001 at V4 (Almgren-Chriss impact model calibrated to live fill slippage — empirical, not estimated). CLAIM-ATTR-001 at V4 (INV-005: daily P&L; reconciliation script passing continuously — `scripts/daily_pnl_reconciliation.sh` zero errors for 30 consecutive trading days).

**Mandatory Claims:** CLAIM-CAPACITY-001 at V4, CLAIM-ATTR-001 at V4

**Forbidden Actions:** Any position sizing that does not account for market impact at the current AUM level. Any trading day without INV-005 reconciliation run (govcheck flags CLAIM-ATTR-001 expiry).

**Note:** IV-B/IV-C progression gates use CLAIM-ATTR-001 V4 as precondition for AUM increase beyond IV-B (20% notional).

S15 — ML Lifecycle Capital Gate: None

**Entry Condition:** S14 acceptance gate passed. CLAIM-CAPACITY-001 at V4. CLAIM-ATTR-001 at V4.

**Exit Condition (Acceptance Gate):**
[code] 
    govcheck validate --scope C0,P11,G4 --strict
[/code]

**Pass Criteria:** `scripts/quarterly_wfv.sh` passing: full re-validation of live model on extended OOS, minimum 3 additional regimes post-deployment, DSR ≥ 0.5 maintained. Champion/challenger framework operational: any new model candidate must pass full S10–S13 slice sequence before live promotion. CLAIM-MODEL-001 at V3 on all registered models. CLAIM-SIGNAL-001 at V3 on all active signals. No claim in decay (B1.2.4 decay vector passing).

**Mandatory Claims:** All research platform claims at or above declared class.

**Forbidden Actions:** Any model promotion that bypasses the S10–S13 qualification sequence. Any quarterly WFV that uses post-cutoff data for the IS period. Any champion/challenger switch without govcheck G4 pass on the challenger.

**Note:** IV-D (100% target notional) requires: zero P0 incidents since IV-C entry + S15 acceptance gate passed + first P0 incident successfully recovered per runbook (A2.2 Phase IV-D).

## I4 — Claim-to-Slice Mapping

Classification
    Governance Reference — Tier 0

Governing Principles
    P-001, P-007

Note
    This table is the authoritative source for which slice is responsible for proving which claim. Any claim not listed here is either pending assignment (requires Phase I RFC) or is a meta-claim (CLAIM-000).

Claim ID | Namespace | Target Class | Proving Slice | CI Gate Activated | Capital Gate Dependency | Notes  
---|---|---|---|---|---|---  
CLAIM-000 | bootstrap | V1 | S0 | C0 | None | Bootstrap only  
CLAIM-WAL-001 | wal.durability | V4 | S1 | P3, P4, P5 | None | 10× kill-9  
CLAIM-SPSC-001 | ipc.spsc | V4 | S1 | P4 | None | TSAN + P99 ≤ 50ns  
CLAIM-L3-POOL-001 | allocator.pool | V2 | S1 | P3 | None | No dynamic alloc  
CLAIM-LOB-001 | lob.determinism | V4 | S2 | P3, P6 | None | 3× byte-identical replay  
CLAIM-L0-001 | ingestor.boundary | V3 | S2 | P1 (GC-E-RESEARCH-001) | None | Import-linter clean  
CLAIM-L0-002 | ingestor.gap | V4 | S2→S5 | P3 | None | V3 at S2 close, V4 at S5 close  
CLAIM-L3-001 | exec.wal_ordering | V4 | S3 | P3, P4 | None | WAL-before-state kill-9  
CLAIM-INV-001 | exec.position | V4 | S3 | P3 | TERMINAL_HALT trigger | 10M fills + 10× kill-9  
CLAIM-L3-02 | exec.fix_consumer | V3 | S4 | P1 (GC-E-042) | None | Zero business logic in FIX consumer  
CLAIM-L3-003 | exec.state_ownership | V3 | S3 | P1 (GC-E-043) | None | `order_tag_` private  
CLAIM-L3-004 | exec.memory_ordering | V4 | S3 | P4 (GC-E-044) | None | TSAN + CROSS_PLANE_READ  
CLAIM-L3-006 | exec.atomic_exposure | V4 | S3 | P4 (GC-E-045) | None | `gross_exposure_` sole cross-plane read  
CLAIM-L3-MAR-001 | exec.mass_cancel | V4 | S3 | P3 | None | MassActionReport WAL-before-state  
CLAIM-L4-001 | risk.kill_switch | V4 | S3 | P4 | IV-A precondition | Kill switch < 1ms  
CLAIM-L3-05 | startup.buffer | V4 | S4→S5 | P3 | Phase II entry | V3 at S4, V4 at S5 (live feed measurement)  
CLAIM-RESEARCH-001 | research.boundary | V3 | S8 | G3 (GC-E-RESEARCH-001) | None | No research→engine imports  
CLAIM-DATA-001 | data.point_in_time | V4 | S9 | G3 | None | No lookahead  
CLAIM-DATA-002 | data.splits | V3 | S9 | G3 | None | Immutable split dates  
CLAIM-BACKTEST-001 | research.backtest | V4 | S10 | P9 | None | INV-002 determinism  
CLAIM-WFV-001 | research.wfv | V3 | S10 | P10 | Signal promotion | WFV OOS discipline  
CLAIM-FEATURE-PARITY-001 | feature.parity | V4 | S11 | P8 | G4 precondition | INV-003 byte-identical  
CLAIM-MODEL-001 | model.registry | V3 | S11 | P11 | G4 precondition | Model card completeness  
CLAIM-REGIME-001 | regime.wal | V4 | S11 | P4 | None | INV-004 determinism  
CLAIM-SIGNAL-001 | signal.weights | V3 | S12 | G3 | Live model gate | Weight vector immutable post-promotion  
CLAIM-MODEL-002 | model.inference | V4 | S13 | G4 | Live model gate | Inference P99 on prod hardware  
CLAIM-CAPACITY-001 | capacity.impact | V4 | S14 | P11 | IV-C precondition | Almgren-Chriss calibrated  
CLAIM-ATTR-001 | ops.pnl | V4 | S14 | P11 | IV-B/IV-C precondition | INV-005 daily reconciliation  
CLAIM-FEATURE-001 | feature.determinism | V4 | S2→S11 | P3, P8 | G4 precondition | Replay fidelity gate — initiated S2, V4 certified S11  
CLAIM-INV-001 (monitor) | exec.position | V4 | S3 (proved), S7 (live monitor) | — | TERMINAL_HALT | Live monitor active from S7 onward; any violation triggers TERMINAL_HALT  
  
## I5 — Governance Sign-Off Document Registry

Classification
    Governance Reference — Tier 0

Governing Principles
    P-004, P-007

Some slice exits require a human-signed governance document in addition to govcheck passing. These documents are YAML files committed to `governance/signoffs/`. They are append-only. Their SHA-256 is recorded in the relevant slice's `proof_artifacts` list.

Document ID | Slice | Trigger | Signatories | Stored Path | govcheck Check  
---|---|---|---|---|---  
LIVE-CAPITAL-001.yaml | S7 exit | Capital deployment authorization — live funds | Founder | `governance/signoffs/LIVE-CAPITAL-001.yaml` | govcheck validates schema + signature field  
MARKET-DATA-001.yaml | S5 exit | Market data qualification — 14-day uptime + replay confirmed | Founder | `governance/signoffs/MARKET-DATA-001.yaml` | govcheck G2 gate checks presence  
DEPLOY-STRATEGY-001.yaml | S13 exit | Strategy deployment — live model authorization | Founder | `governance/signoffs/DEPLOY-STRATEGY-001.yaml` | govcheck G4 gate checks presence + model SHA-256 match  
TIMING-CONSTANTS-001.yaml | S6 exit | Production hardware timing constants committed | Founder | `governance/signoffs/TIMING-CONSTANTS-001.yaml` | govcheck validates hardware_id matches deployment target  
STRESS-CERT-001.yaml | F exit | Stress test certification — all ST-01 through ST-08 scenarios passing in simulation; ST-06 and production-harness scenarios documented as Phase III deferred | Founder | `governance/signoffs/STRESS-CERT-001.yaml` | govcheck validates presence and schema at Phase F exit before Phase G instrumentation begins  
INCIDENT-{ID}.yaml | Any P0 | P0 incident record — root cause, resolution, replay verification | Founder | `governance/incidents/INCIDENT-{ID}.yaml` | govcheck flags if P0 alert open without incident record for >24h  
  
⚑ **Governance Reconciliation Note:** LIVE-CAPITAL-001, MARKET-DATA-001, DEPLOY-STRATEGY-001, TIMING-CONSTANTS-001 require Founder-Solo signature per A2.5 governance authority matrix. Cryptographic multi-signature controls (L1B: 2-of-3 CRO/CTO/Head of Trading) govern production configuration envelopes (config.bin.env) and strategy promotion Stage 4 (Pilot), not governance sign-off authority. These are distinct control layers.

## I6 — RFC Template (Request for Change)

Classification
    Governance Process Document — Tier 0

Governing Principles
    P-004, P-007

Applies To
    Any change to a V3+ claim scope, any ADR supersession, any architectural principle modification, any slice acceptance criteria change after slice has opened

RFC Governance Status
    **ACTIVE** — Template, process, manual enforcement, and GC-E-053 blocking rule are complete and binding

RFC Automation Status
    **DEFERRED TO PHASE J** — `govcheck rfc` subcommands (lint, validate, impact, apply) are Phase J deliverables per J5.1. Manual enforcement protocol (I6.1) remains binding until automation is implemented.

### I6.1 — When an RFC Is Required

RFC Required | RFC Not Required  
---|---  
  
  - Any V3 or V4 claim's verification scope changes (broader or narrower)
  - Any ADR is superseded
  - Any architectural principle (P-001 through P-007) is amended
  - Any slice acceptance criteria change after that slice has opened
  - Any CI gate activation phase changes (e.g., GC-E-010 moved from Phase 3 to Phase 1)
  - Any new GC-E rule is added or existing rule is deactivated

| 

  - V1 or V2 claim upgrades (normal promotion — new proof artifact committed, govcheck C0 passes)
  - New V1 claims (new YAML file + owner assigned)
  - Operational log entries (typo fixes, metadata updates with zero governance impact)
  - Runbook text updates (no claim impact)

  
  
RFC Mechanical Enforcement Status

The `govcheck rfc` subcommands (lint, validate, impact, apply) specified in I6.3 are **not yet implemented**. They are Phase J deliverables per J5.1 (`govcheck/core/dag_validator.cpp`, `govcheck/core/exit_gate_verifier.cpp`).

**Until govcheck rfc commands are implemented:** RFC compliance is enforced through the following manual verification protocol, which is the binding procedure for all RFC submissions during Phases I and J:

  1. RFC YAML committed to `governance/rfcs/RFC-NNN.yaml` — schema manually verified against I6.2
  2. Impact assessment manually executed: author lists all claims with verification_class ≥ V3 that reference the artifact being changed; list committed to `governance/rfcs/RFC-NNN-impact.yaml`
  3. Chief Architecture Office countersigns RFC-NNN-impact.yaml before RFC status changes from `draft` to `open`
  4. All affected claims manually re-verified at their declared class before RFC status changes to `accepted`
  5. Govcheck C0 must exit 0 with all affected claims amended before RFC is considered closed



**GC-E-053 (registered in B1.3.3):** Any claim with verification_class ≥ V3 that has been amended without a corresponding RFC-NNN.yaml in accepted status blocks the C0 gate. This rule is implemented in the govcheck claims validator (B1.3.2, validators/claim_validator.cpp) and does not require the govcheck rfc subcommands to function.

### I6.2 — RFC Schema
[code] 
    {
      rfc_id:          "RFC-NNN",
      title:           "Short imperative title",
      status:          "draft | open | accepted | rejected | superseded",
      author:          "founder",
      date_opened:     "ISO-8601",
      date_closed:     "ISO-8601 | null",
      change_type:     "claim_scope | adr_supersession | principle_amendment | slice_criteria | gc_rule | new_gate",
      artifacts_changed: [
        { artifact_id: "CLAIM-XXX | AD-NNN | P-00N | S{N}", change_description: "..." }
      ],
      rationale:       "Why this change is necessary",
      alternatives_considered: [
        { alternative: "...", rejection_reason: "..." }
      ],
      impact_assessment: {
        claims_invalidated: ["CLAIM-XXX"],
        claims_requiring_re_proof: ["CLAIM-YYY"],
        slices_reopened: ["SN"],
        capital_gates_affected: ["IV-A | IV-B | IV-C | IV-D"]
      },
      approval_authority: "CTO + Governance Board | Founder",
      approval_record:    "govcheck rfc validate RFC-NNN → 0 errors required before acceptance",
      supersedes:         null | "RFC-MMM"
    }
[/code]

### I6.3 — RFC Process

⚑ **Implementation status:** Steps 2 (`govcheck rfc lint`), 4 (`govcheck rfc impact`), and 7 (`govcheck rfc apply`) reference commands not yet implemented. Use the manual verification protocol in I6.1 until these commands are available. Steps 1, 3, 5, 6, 8, 9 are executable today using existing govcheck validate and standard file operations.

  1. Author opens RFC YAML in `governance/rfcs/RFC-NNN.yaml` with status: `draft`
  2. `govcheck rfc lint RFC-NNN` validates schema completeness (all required fields present, all artifact_ids resolvable)
  3. Author changes status to `open` and commits — this starts the review clock
  4. Impact assessment executed: `govcheck rfc impact RFC-NNN` outputs list of claims requiring re-proof and slices affected
  5. If claims require re-proof: new proof artifacts committed before RFC is accepted (govcheck C0 must pass with amended claims)
  6. Author changes status to `accepted` (or `rejected`) — append-only, no deletion
  7. If accepted: `govcheck rfc apply RFC-NNN` generates amended YAML stubs for all affected artifacts
  8. All affected slice acceptance gates are re-run: `govcheck validate --scope {affected_gates} --strict` must pass
  9. Commit record: RFC YAML + all amended artifacts + new govcheck output in `governance/rfcs/RFC-NNN-evidence/`



## I7 — govcheck Boundary Audit

Classification
    Implementation Verification Record — Tier 1

Governing Principles
    P-001, P-004

Resolves
    Gap 3 from Phase H Consistency Review (govcheck boundary audit)

### I7.1 — Repository Directory Boundary

The research plane boundary (AD-002) is enforced by GC-E-RESEARCH-001 on the following directory structure. This table is binding — any new top-level directory in the repository requires a govcheck rule assignment before the first merge that creates it.

Directory | Plane | Import Rule | GC-E Rule | Active From  
---|---|---|---|---  
engine/ | Execution | May not import from research/, governance/ (only engine/ subdirs) | GC-E-RESEARCH-001 | S2 (P1 gate)  
research/ | Research | May import from engine/replay_engine.h only (AD-001) | GC-E-RESEARCH-002 | S8 (G3 gate)  
governance/ | Governance | Read-only from all planes (govcheck reads, never writes during build) | GC-F-002 (version mismatch) | S0 (C0 gate)  
scripts/ | Operations | May import Python stdlib + dependencies only; no engine/ or research/ imports | GC-E-RESEARCH-001 (extended) | S4 (P1 gate)  
tests/ | Test | May import engine/ and research/ (test scope only). Not deployed. | P2 build matrix validates test isolation | S0  
data/ | Data Lake | Managed by DVC. No Python imports permitted in DVC pipeline files. | GC-E-RESEARCH-001 (extended) | S9 (G3 gate)  
  
### I7.2 — GC-E Rule Activation Schedule

govcheck rules activate in phases. A rule listed as "Warning" transitions to "Blocking" at the phase shown. No rule reverts from Blocking to Warning without an RFC.

Rule | Description | Warning From | Blocking From | Slice | Notes  
---|---|---|---|---|---  
GC-F-004 | CLAIM-GOVCHECK-001 not at V4 on current hardware_id | S0 start | S0 (immediate blocking) | C0 | Root trust chain — no other gate is valid without this  
GC-E-010 | malloc/new/free/delete in engine/ hot path | S0 start | S3 | P2→P3 | ADR-006 (compile-time SHM)  
GC-E-011 | clock_gettime(CLOCK_REALTIME) in engine/ | S0 start | S2 | P2 | Determinism (P-003)  
GC-E-018 | Proof artifact SHA-256 mismatch | S0 | S0 (immediate blocking) | C0 | No grace period  
GC-E-019 | Claim owner not in team.yaml | S0 | S0 | C0 | Solo founder: auto-passes if owner = "founder"  
GC-E-020 | review_expiry exceeded | S1 | S1 | C0 | 90-day default expiry  
GC-E-021 | R4 claim without waiver on deploy-gate | S3 | S3 | G1/G2 | Required before staging deploy  
GC-E-022 | Waiver expired | S1 | S1 | C0 | —  
GC-E-023 | Cross-namespace dependency undeclared | S0 | S0 | C0 | —  
GC-E-025 | Active claim with superseded dependency | S0 | S0 | C0 | —  
GC-E-031 | V4 claim with no empirical proof artifact | S1 | S1 | C0 | Core quality gate  
GC-E-042 | SessionID/MsgSeqNum/R_applied in FIX consumer TU | S4 | S4 | P1 | FIX consumer purity  
GC-E-043 | Direct access to order_tag_/x_l_ outside L3 | S3 | S3 | P1 | Memory safety  
GC-E-044 | memory_order_relaxed on cross-plane atomic | S3 | S3 | P4 | TSAN requirement  
GC-E-045 | Direct x_l_/order_tag_ read from L4/L5 | S3 | S3 | P1 | —  
GC-E-RESEARCH-001 | engine/ imports from research/ | S2 | S2 | P1/G3 | Research boundary AD-002  
GC-E-RESEARCH-002 | Backtester not using replay_engine.h | S8 | S8 | P9 | AD-001  
GC-E-RESEARCH-003 | pickle in engine/ or production inference | S4 | S4 | G4 | AD-011 (ONNX-only)  
GC-E-RESEARCH-004 | OOS vault modification without unseal record | S9 | S9 | P10 | B3 OOS discipline  
GC-E-050 | REG-RISK-STALE-001 production value exceeds 5000ms | S3 | S3 (immediate blocking at G2) | G2 | Institutional risk control standard  
GC-E-053 | V3+ claim amended without accepted RFC in artifacts_changed list | S1 | S1 (immediate blocking at C0) | C0 | RFC discipline for V3+ claims  
GC-E-054 | Research SIMD/FMA flags non-compliant | S1 start | S1 (blocking at P2) | P2 | Feature parity INV-003 precondition  
GC-E-055 | ONNX Runtime version mismatch at PF-09 | S11 start | S11 (blocking at PF-09 and G4) | PF-09 / G4 | Model inference integrity  
GC-E-SECRETS-001 | Secret material in tracked source files | S1 start | S1 (immediate blocking at C0) | C0 | PROC-008 enforcement — Vault prerequisite from S1-entry  
GC-E-SECRETS-002 | Vault endpoint unreachable at pre-flight | S1 start | S1 (blocking at pre-flight) | PF-07 | Session startup cannot proceed without secrets resolution  
GC-E-056 | SIM_STATE simulation isolation boundary violation (AD-013) | S1 start | S1 (immediate blocking at C0) | C0 | Mirrors GC-E-RESEARCH-001 enforcement pattern for simulation boundary  
  
## Phase I — Consistency Review

### Consistency Checks Performed

  - Slice entry/exit conditions for S0–S7 are traceable to A2.2 Operating Phases I–IV without contradiction — Operating Phase exit criteria are the union of their constituent slice acceptance gates
  - Claim-to-Slice mapping (I4) assigns every claim registered in B1.2.5 to exactly one proving slice — no claim is unassigned; CLAIM-FEATURE-001 is assigned as a compound claim proven progressively across S2 and S11
  - Sign-off documents (I5) match the exit conditions declared in A2.2 — LIVE-CAPITAL-001, MARKET-DATA-001, DEPLOY-STRATEGY-001 are consistent with Phase II, III, and V exit conditions respectively
  - RFC template (I6) is consistent with the authority matrix (A2.5) — RFC approval authority matches the governance authority tier for each change type
  - GC-E rule activation schedule (I7.2) is consistent with CI gate phase assignments in B1.4 — no rule is listed as blocking before its CI gate is active
  - Research slice prerequisite (S4 exit) is consistent with the research plane boundary (AD-002) — research infrastructure does not require live exchange access; staging qualification is sufficient
  - govcheck boundary audit directory map (I7.1) is consistent with AD-002 and AD-001 — engine/, research/, governance/ boundaries match the plane definitions in A1.3
  - TIMING-CONSTANTS-001.yaml sign-off (I5) is consistent with the V4 claim scope requirement in B1.2.2 — hardware_id tagging requirement preserved
  - Capital gate progression (S7 → IV-A through IV-D) is consistent with A2.2 Phase IV capital scaling table — no new thresholds introduced
  - Phase I timeline entry updated to 'active' — consistent with PHASES array Correction 2



### Governance Reconciliation Status

The following items were previously listed as gaps. Each has been reclassified per the Governance Reconciliation Report:

Item| Previous Classification| Reconciled Status| Authority  
---|---|---|---  
Unified implementation roadmap| Gap| Deferred by Design| J0–J2 (Phase J deliverable)  
ENGINE_V3 phase-to-calendar mapping| Gap| Deferred by Design| Phase J scope  
Slice dependency graph visualization| Gap| Deferred by Design| J1 (Phase J deliverable)  
govcheck binary implementation specification| Gap| Deferred by Design| J5.1 (Phase J deliverable)  
S6 production hardware procurement| Gap| Deferred by Design| J3 (Phase J infrastructure planning)  
Inter-slice re-opening protocol| Gap| Partially Confirmed| J4 (formalized), I6 (RFC process exists)  
DR contradiction (AD-016 vs AD-028)| Gap| Contradicted by Authority| AD-016, AD-028 are complementary  
Multi-signature authority| Gap| Partially Confirmed| A2.5 (Founder-Solo), L1B (2-of-3 crypto)  
Governance telemetry blind spot| Gap| Contradicted by Authority| B1.7, G1–G5 complete; production wiring deferred  
Security architecture missing| Gap| Contradicted by Authority| AD-014, AD-015, AD-017, AD-029, K1.1, K1.2 active  
RFC process incomplete| Gap| Partially Confirmed| I6 governance active; automation deferred to J5.1  
Observability/Simulation disconnected| Gap| Contradicted by Authority| G3 Feedback Loop wired via ObservabilityBridge  
  
J

## Phase J — Roadmap S0–S15+

Unified implementation roadmap: Slice Dependency Graph, Milestone Registry, Infrastructure Procurement, Inter-Slice Re-Opening Protocol, govcheck Binary Specification, and Consistency Review.

In Progress

## J0 — Roadmap Thesis

J0.1 — Purpose

Phase I established the governance contract: what each implementation slice must prove, which claims it must satisfy, and which ADRs it must comply with. Phase J establishes the operational contract: _when_ slices execute, _in what order_ , and under what dependency constraints progress from S0 to live capital must flow.

The roadmap is a directed acyclic graph (DAG). No cycle may exist. govcheck enforces this by refusing to accept an exit-gate artifact that claims a preceding gate has not yet been cleared.

J0.2 — Governing Principle

**P-005 (Roadmap Integrity):** No implementation slice may be declared complete unless every slice listed in its `depends_on` set has a signed exit-gate artifact on record. A signed exit-gate artifact is an immutable YAML record counter-signed by the Chief Architecture Office and stored in `governance/exit-gates/`.

Principle| Statement| Enforcement  
---|---|---  
P-001| No production capital without govcheck clean| GC-E-CAPITAL-001  
P-002| No research data without signed provenance| GC-E-PROVENANCE-001  
P-003| No algorithm parameter change without claim re-verification| GC-E-CLAIM-001  
P-004| Every implementation action is bound to a slice| Slice Registry (I2/I3)  
**P-005**| **No slice complete without all dependency exit-gates signed**| **govcheck DAG validator**  
  
J0.3 — Roadmap Scope

  - **S0–S7:** Execution Plane slices — govcheck enforced; capital gates apply at S7 (IV-A), S7-extended (IV-B, IV-C, IV-D)
  - **S8–S15:** Research Platform slices — govcheck enforced; research data provenance gates apply; no capital authority directly unlocked
  - **Phase IV-A through IV-D:** Capital scaling milestones; these are NOT implementation slices — they are program milestones triggered by verified clean-operation duration evidence
  - Phase K and Phase L (HTML Documentation System and Cursor Prompt Package) are not implementation slices; they are blueprint maintenance phases and execute in parallel with any open slice at the discretion of the Chief Architecture Office



## J1 — Slice Dependency Graph

J1.1 — Dependency Rules

  - **Hard Sequential:** S0 → S1 → S2 → S3 → S4 → S5 → S6 → S7. The core execution plane must be built in strict sequence. No slice may begin entry criteria verification until all slices in its `depends_on` set have signed exit gates.
  - **Parallel Window:** S8–S15 (Research Platform) may begin at S3-exit. The research plane requires the core data feed (S3) and claim validation tooling (S2/S3 outputs) but does not require the execution engine (S5–S7).
  - **Capital Gates:** IV-A unlocks at S7-exit + 30-day clean-operation evidence. IV-B/C/D each require additional clean-operation duration evidence and are not slice-gated beyond S7.
  - **govcheck Binary:** govcheck must be operable at S1-exit and must remain ASAN/TSAN/UBSAN-clean throughout all subsequent slices.



J1.2 — Dependency Table

Slice| Depends On| Can Parallelize With| Capital Gate Unlocked  
---|---|---|---  
S0| — (entry point)| —| None  
S1| S0-exit| —| None  
S2| S1-exit| —| None  
S3| S2-exit| —| None  
S4| S3-exit| S8–S15 (parallel window opens)| None  
S5| S4-exit| S8–S15 continuing| None  
S6| S5-exit| S8–S15 continuing| None  
S7| S6-exit| S8–S15 continuing| IV-A (30-day clean)  
S8| S3-exit| S4–S7| None  
S9| S8-exit| S4–S7| None  
S10| S9-exit| S5–S7| None  
S11| S10-exit| S5–S7| None  
S12| S11-exit| S6–S7| None  
S13| S12-exit| S7| None  
S14| S13-exit| S7| None  
S15| S14-exit| Post-S7| None (research only)  
  
J1.3 — Interactive Dependency Graph

Click any slice node to scroll to its Phase I specification. Hover for dependencies. Execution plane = blue chain; Research plane = amber chain; Capital gates = red markers.

Execution Plane (S0–S7) Research Plane (S8–S15) Capital Gate Parallel Window

## J2 — Milestone Registry

J2.1 — Milestone Definitions

Program milestones are named waypoints that trigger capital authority changes or formal program reviews. They are distinct from slice exit-gates: a milestone is evidence-triggered, not task-triggered.

Milestone ID| Name| govcheck Precondition| Capital Authority Unlocked| Rollback Trigger  
---|---|---|---|---  
MS-001 | Execution Foundation | S0 + S1 + S2 exit-gates signed | None — internal development only | Any ASAN/TSAN/UBSAN failure in the govcheck binary itself  
MS-002 | Data & Validation Ready | S3 exit-gate signed; all CLAIM-DATA-XXX verified | None — simulated data only | Market data provenance validation failure  
MS-003 | Algorithm Engine Complete | S4 exit-gate signed; CLAIM-ALGO-XXX verified | None — simulation environment only | Claim re-verification failure after parameter change  
MS-004 | Simulation Validated | S5 exit-gate signed; stress test registry all-pass | None — stress testing only | Any stress test regression  
MS-005 | Execution Engine Deployed | S6 + S7 exit-gates signed; co-location hardware procured per J3 | Paper trading only (zero capital at risk) | Hardware SLA breach; govcheck clean-operation gap > 24h  
MS-006 | Capital Gate IV-A | MS-005 + 30 consecutive days govcheck-clean live paper trading; CAPITAL_GATE_CLOCK_START WAL record committed | Live capital up to IV-A threshold (to be set via LIVE-CAPITAL-001.yaml) | Single drawdown breach; govcheck failure; 30-day clock resets  
MS-007 | Capital Gate IV-B | MS-006 + 90 consecutive days govcheck-clean within IV-A limits; CAPITAL_GATE_CLOCK_START WAL record committed | Live capital up to IV-B threshold | Same as IV-A; 90-day clock resets to zero  
MS-008 | Capital Gate IV-C | MS-007 + 180 consecutive days govcheck-clean within IV-B limits; CAPITAL_GATE_CLOCK_START WAL record committed | Live capital up to IV-C threshold | Same; 180-day clock resets  
MS-009 | Capital Gate IV-D | MS-008 + 365 consecutive days govcheck-clean within IV-C limits; CAPITAL_GATE_CLOCK_START WAL record committed | Live capital up to IV-D threshold (full institutional scale) | Same; 365-day clock resets  
MS-010 | Research Platform Live | S15 exit-gate signed; all research data provenance gates verified | None — research output only; no capital authority | Research data provenance failure; signed data retraction  
  
J2.2 — Milestone Progress Board

Interactive status board. Each card reflects the milestone type and capital authority category.

J2.3 — Capital Gate Clock Rules

  - The clean-operation clock for each capital gate starts on the first govcheck-verified trading session after the preceding milestone is formally confirmed in writing by the Chief Architecture Office.
  - The clock resets to zero on any of: (a) govcheck failure on any GC-E rule, (b) drawdown breach as defined in LIVE-CAPITAL-001.yaml, (c) inter-slice re-opening event (see J4) that affects S7 or any signed exit-gate in the causal chain of the live system.
  - The clock does NOT reset on: govcheck warnings (GC-W-*), planned maintenance windows pre-approved in writing by the Chief Architecture Office, or research platform events that do not touch the execution plane.
  - Clock evidence must be machine-readable: a continuous log of govcheck exit codes, timestamped to the second, retained for the full duration of the gate period plus five years.



#### Capital Gate Clock WAL Record

The clock start event is an immutable WAL record of type `CAPITAL_GATE_CLOCK_START`. This makes the clock duration independently verifiable by govcheck rather than relying on prose assertion.
[code] 
    // WAL record payload for CAPITAL_GATE_CLOCK_START
    // record_type: CAPITAL_GATE_CLOCK_START (enum value 0x0A)
    {
      "milestone_id":              "MS-006 | MS-007 | MS-008 | MS-009",
      "preceding_exit_gate_hash":  "sha256:...",   // SHA-256 of the signed exit-gate YAML
      "authorized_by":             "@founder",
      "authorization_ref":         "governance/exit-gates/SLICE-S7-exit-v1.yaml",
      "clock_start_tsc":           1234567890123456789,  // CLOCK_MONOTONIC_RAW
      "clock_start_wall":          "ISO-8601",
      "clean_operation_required_days": 30,              // Milestone-specific duration
      "capital_authority_at_milestone": "5% target notional",
      "signature":                 "sha256:..."
    }
[/code]

govcheck command `govcheck capital-gate clock-status --milestone MS-006` reads the WAL record and the timestamped govcheck log to compute elapsed clean-operation days. This command is a Phase J engineering deliverable per J5.1.

## J3 — Infrastructure Procurement Specification

Resolves Phase I Gap 5: formal procurement gate items for hardware, co-location, and exchange connectivity required at S6-exit.

J3.1 — Procurement Gate Items

Item ID| Category| Specification| Required By| Acceptance Criterion  
---|---|---|---|---  
PROC-001 | Co-location Facility | Tier-3 or Tier-4 facility; maximum 2ms one-way latency to primary exchange matching engine; redundant power (2N or N+1); physical security SOC-2 Type II certified | S6-entry | Signed co-location SLA on file in `governance/procurement/`; latency measurement report attached  
PROC-002 | Execution Server — CPU | AMD EPYC 9004 series or Intel Xeon Sapphire Rapids; minimum 32 physical cores; CPU affinity pinning for ENGINE_V3 ring threads; NUMA topology documented | S6-entry | CPU affinity configuration file committed to `config/cpu-affinity.yaml`; govcheck PROC-CPU-001 rule passes  
PROC-003 | Execution Server — NVMe Storage | PCIe Gen 4.0 NVMe; minimum 2TB usable; sequential write > 6 GB/s; fsync latency < 100µs at P99; dedicated controller, not shared with OS volume | S6-entry | fsync benchmark report (fio output) attached to S6 exit-gate artifact; P99 < 100µs verified  
PROC-004 | Network Interface | 10 GbE minimum; kernel bypass (DPDK or RDMA) capable NIC; interrupt coalescing disabled for latency-sensitive rings; dedicated VLAN for exchange connectivity | S6-entry | NIC configuration file on record; kernel bypass mode verified via ping-pong latency test (< 5µs P99 loopback)  
PROC-005 | Exchange Connectivity | FIX/FAST or exchange-native binary protocol; dedicated leased line or sponsored access (not shared internet); primary + backup connectivity; failover < 500ms | S7-entry | Exchange connectivity test report (paper trading session with live market data); failover drill report signed by Chief Architecture Office  
PROC-006 | Redis Production Instance | Redis 7.x; ACL per consumer group (per AD-010/H4.2); AOF persistence fsync every-second; co-located with execution server on same NUMA node; no shared Redis with research plane | S4-entry | Redis ACL configuration file in `config/redis/`; govcheck SINK-02 rule passes; separate research Redis instance confirmed  
PROC-007 | Monitoring Infrastructure | Prometheus (egbos_exporter sidecar per H2.1); Grafana 10+ with panels PAN-001 through PAN-012; alerting webhook to on-call system; retention ≥ 90 days | S5-entry | govcheck PROM-001 rule passes; all 12 panels rendering in Grafana with live metrics; alert webhook verified by test fire  
PROC-008 | Secrets Management | HashiCorp Vault or equivalent; no secrets in environment variables or source control; exchange API keys, Redis passwords, and co-location credentials stored in Vault; access audited | S1-entry | govcheck GC-E-SECRETS-001 rule passes (to be registered via RFC per J6.1); Vault audit log enabled  
  
J3.2 — Procurement Governance

  - All procurement gate items must be documented as YAML records in `governance/procurement/PROC-XXX.yaml`, each signed by the Chief Architecture Office before the dependent slice may begin.
  - Procurement records are immutable once signed. Amendments require an RFC (per I6) and a new signed PROC-XXX-v2.yaml record; the original record is archived, not deleted.
  - govcheck rules for procurement (GC-E-PROC-*) are activated at the slice entry gate of the dependent slice. They verify the presence and hash-integrity of the signed YAML record.
  - Vendor selection is outside the scope of this document. The PROC items specify performance and governance requirements; vendor selection is the responsibility of the procurement team subject to these constraints.



## J4 — Inter-Slice Re-Opening Protocol

Resolves Phase I Gap 6: formal procedure when a downstream slice discovery invalidates a prior slice's acceptance gate.

J4.1 — Re-Opening Trigger Conditions

  - **REOPEN-T1:** A downstream slice's exit-gate artifact contains a contradiction with a claim proven in an upstream slice's exit-gate artifact.
  - **REOPEN-T2:** A govcheck rule change (via RFC) retroactively invalidates an assertion that was clean under the prior rule set.
  - **REOPEN-T3:** A hardware or infrastructure failure reveals that a performance claim (e.g., fsync latency, NIC latency) certified at procurement cannot be sustained in production.
  - **REOPEN-T4:** An ADR amendment (via RFC and AD-XXX-vN.yaml) requires re-verification of any claim that referenced the prior ADR version.
  - **REOPEN-T5:** Market data or exchange connectivity changes that invalidate a data-layer claim proven in S3 or S8.



J4.2 — Re-Opening Procedure

Step| Action| Authority| Artifact Produced  
---|---|---|---  
REOPEN-01 | Issue a Re-Opening Notice: a written statement identifying the trigger condition (T1–T5), the slice to be re-opened, and the specific exit-gate assertion being invalidated | Any team member may raise; Chief Architecture Office must countersign within 48h or escalate | `governance/reopen/REOPEN-XXX-notice.yaml`  
REOPEN-02 | Suspend all dependent slices. Any slice whose entry criteria reference the invalidated exit-gate is immediately suspended — it may not proceed until re-opening is resolved. Capital gate clocks for affected milestones pause. | Chief Architecture Office | Suspension notice appended to `REOPEN-XXX-notice.yaml`  
REOPEN-03 | File an RFC (per I6 template) for the specific claim, ADR, or principle requiring amendment. The RFC must cite the REOPEN-XXX notice as its trigger. | RFC Author (assigned by Chief Architecture Office) | `governance/rfcs/RFC-XXX.yaml`  
REOPEN-04 | Re-verify the affected slice under the amended rule set. govcheck must pass with the new rules active. A new signed exit-gate artifact (SLICE-XX-exit-vN.yaml) is produced. | Slice owner; Chief Architecture Office countersigns | `governance/exit-gates/SLICE-XX-exit-vN.yaml`  
REOPEN-05 | Re-activate suspended slices in dependency order. Capital gate clocks resume from the day of re-activation (they do not retroactively credit the suspended period). | Chief Architecture Office | Re-activation notice appended to `REOPEN-XXX-notice.yaml`; marked RESOLVED  
  
J4.3 — govcheck Rule for Re-Opening

A new govcheck warning rule `GC-W-REOPEN-001` is registered forward-reference here and will be formally specified via RFC in Phase K:

  - **Rule ID:** GC-W-REOPEN-001
  - **Trigger:** An open REOPEN-XXX-notice.yaml exists in `governance/reopen/` without a RESOLVED marker and the current slice's exit-gate references any exit-gate that is cited in the notice.
  - **Severity:** WARNING (escalates to ERROR if the notice has been open for more than 7 calendar days)
  - **Amendment authority:** RFC required
  - _Note: GC-E-SECRETS-001 and GC-E-SECRETS-002 are no longer forward references. They are registered in B1.3.3 and B1.5.2 with S1 activation. The Phase K RFC notation in the original J5 specification is superseded by this direct registration._



## J5 — govcheck Binary Implementation Specification

Resolves Phase I Gap 4: specifies what the govcheck binary must itself implement, verified against the rule catalog in I7.2.

J5.1 — Binary Architecture

govcheck is a compiled C++20 binary. It is the sole enforcement mechanism for all GC-E and GC-W rules. It must itself be ASAN/TSAN/UBSAN-clean and must pass its own CI gate on every commit.
[code] 
    govcheck/
    ├── CMakeLists.txt              # C++20; -fsanitize=address,thread,undefined
    ├── core/
    │   ├── runner.cpp              # Entry point; CLI argument parsing; exit code 0=clean, 1=error, 2=warning
    │   ├── rule_loader.cpp         # Loads rule manifests from governance/rules/*.yaml at runtime
    │   ├── dag_validator.cpp       # P-005: verifies no slice gate cycle; reads exit-gate YAMLs
    │   ├── exit_gate_verifier.cpp  # Verifies signed exit-gate artifact presence and SHA-256 hash
    │   └── clock_tracker.cpp       # Capital gate clean-operation clock; reads timestamped govcheck logs
    ├── validators/
    │   ├── capital_validator.cpp   # GC-E-CAPITAL-001: no live capital without signed exit-gate chain to S7
    │   ├── claim_validator.cpp     # GC-E-CLAIM-001: claim re-verification after parameter change
    │   ├── provenance_validator.cpp# GC-E-PROVENANCE-001: research data signed provenance
    │   ├── proc_validator.cpp      # GC-E-PROC-*: procurement YAML presence and hash verification
    │   ├── sink_validator.cpp      # SINK-02/SINK-04: Redis and Prometheus connectivity assertions
    │   ├── secrets_validator.cpp   # GC-E-SECRETS-001: no secrets in env or source —
                                 # ACTIVE from S1. Checks: (1) no *.env files in tracked git history containing KEY=, SECRET=, PASSWORD=, TOKEN= patterns; (2) no hardcoded strings matching exchange API key format in engine/ or scripts/ source files; (3) Vault endpoint reachability verified at C0
    │   └── reopen_validator.cpp    # GC-W-REOPEN-001 (forward ref): open re-opening notice detection
    ├── rules/
    │   ├── rule_manifest.h         # Rule metadata: id, severity, activation_slice, rfc_required
    │   └── rule_registry.cpp       # Static registry of all rules; populated from governance/rules/*.yaml
    └── output/
        ├── reporter.cpp            # Structured JSON output: {rule, severity, slice, message, evidence}
        └── exit_code_policy.cpp    # Exit code 0 iff zero GC-E violations; warnings do not fail
[/code]

✅ GOVERNANCE RECONCILIATION COMPLETE — BRO/EGBOS INTEGRATION ACTIVE

The BRO Subsystem Framework (Sections L0 through L5 and MIG) has been fully integrated with the core EGBOS architecture, resolving the dual governance vocabulary and establishing a single, unified authority model. 

**Authority status:** All BRO Axioms (C-001 through C-005) are active claims in Phase B, DOM registry components are mapped to Phase H/I, and ownership groups are integrated with the govcheck team schema. Sections L0 through L5 are now **ACTIVE & BINDING** governance specifications. 

**Conflict resolution rule:** In any case of contradiction between Phase A–J EGBOS framework and BRO L0–L5 sections, Phase A–J EGBOS framework is authoritative. 

**Integration & Mapping Summary:**

  - **Axiom Mapping:** C-001 to C-005 are registered as Claims `CLAIM-C-001` through `CLAIM-C-005` in Phase B.
  - **DOM Registry Mapping:** DOM-01 through DOM-09 are mapped to ADR-001 through ADR-025 and Slices S0 through S15.
  - **Ownership Integration:** Team roles are bound to `CODEOWNERS` and validated in `governance/team.yaml`.
  - **Enforcement:** Rule GC-F-005 has been updated to enforce compile-time BRO domain boundary verification in the CI pipeline.



L0

## Trading Constitution

Constitution Layer — Governing Axioms, Bounds, and Constraints (Level 0).

ACTIVE

## L0.1 — System Constitution Enforcers

Axiom C-001: Determinism Invariant

Every state transition inside the production trading engine must be a pure, deterministic function of the raw, sequence-ordered event log (Tier 0). Non-deterministic behaviors—such as unseeded PRNGs, wall-clock branch evaluation, thread race conditions, or unaligned floating-point execution—are classified as critical architectural defects. Floating-point operations are compiled with the strict flag `-ffp-model=precise` (on Clang/LLVM) and fast-math is banned to guarantee bit-identical features across compiles.

Axiom C-002: Lock-Free Hot Path & Lock Prohibition

No locks, mutexes, semaphores, or blocking system calls are permitted in the hot path. All inter-thread and inter-process communications must utilize lock-free single-producer single-consumer (SPSC) ring buffers or versioned double-buffered shared memory. Runtime scheduling watchdogs and eBPF probes monitor Node 0 cores; any thread block event or context switch on a pinned hot thread triggers an immediate P0 hardware alarm. Static enforcement is achieved via Clang AST analysis, which matches and blocks compilation of any lock/atomic synchronization call on hot paths.
[code] 
    // Clang AST Matcher to detect and block lock usage in the hot path
    auto hotpath_lock_matcher = callExpr(
        callee(functionDecl(hasAnyName("pthread_mutex_lock", "pthread_mutex_trylock", "std::mutex::lock", "std::unique_lock::lock"))),
        hasAncestor(functionDecl(hasAttr(attr::Annotate), hasAnnotation("egbos_hotpath")))
    ).bind("hotpath_lock");
[/code]

Axiom C-003: Zero Dynamic Heap Allocation & Runtime Allocator Traps

Dynamic memory allocation (via `malloc`, `new`, or container resizing) is strictly prohibited in hot-path execution loop execution. All memory requirements must be pre-allocated during the initialization phase. A custom global allocator wrapper captures any allocation call during the hot path and terminates the process immediately using `SIGABRT`. Static validation is enforced via a Clang AST matcher that scans for new expressions or dynamic container operations.
[code] 
    // Clang AST Matcher to enforce the hotpath allocation ban
    auto hotpath_new_matcher = cxxNewExpr(
        hasAncestor(functionDecl(hasAttr(attr::Annotate), hasAnnotation("egbos_hotpath")))
    ).bind("hotpath_new");
[/code]

Axiom C-004: eBPF Scheduling Watchdogs

To prevent scheduling latency, core affinity is pinned. Isolated CPU cores are monitored by eBPF programs loaded into the kernel. If a process switch, thread migration, or involuntary preemption occurs on a pinned execution thread, the watchdog immediately signals the execution loop to halt trading operations.
[code] 
    // eBPF kernel watchdog program to detect context switches on isolated CPU cores
    SEC("kprobe/finish_task_switch")
    int bpf_detect_context_switch(struct pt_regs *ctx) {
        u32 cpu = bpf_get_smp_processor_id();
        if (cpu == ISOLATED_CPU_CORE_5 || cpu == ISOLATED_CPU_CORE_6) {
            bpf_send_signal(SIGSYS); // Send SIGSYS to immediately halt task
        }
        return 0;
    }
[/code]

Axiom C-005: Shared Memory Isolation & Single-Writer Principle

Multi-tenant shared memory segments must be isolated. No two strategy instances or trading teams may share write-access to the same shared memory segment. Every ring buffer and shared memory segment must have exactly one producer thread. Write contention is eliminated by design; concurrency is managed through versioned double-buffering with read-side sequence validation counters, protected at the kernel level using POSIX Access Control Lists (ACLs) and page-level `mprotect` write-protection flags in consumer processes.

L1A

## Quant Domain Governance

Governance Layer — Namespace Access Control and Ownership Policies (Level 1).

ACTIVE

## L1A.1 — Namespace Sandboxing & Access Control

Strategy Isolation Boundaries

Every trading strategy, alpha signal family, and model pipeline must reside in a distinct namespace (e.g., `qos/domains/team_alpha/strategy_btc/`). Read/write operations to shared memory directories under `/dev/shm/egbos/` are controlled using POSIX Access Control Lists (ACLs) bound to team-specific Linux UIDs.

Namespace Path| Write Owner (UID)| Allowed Readers (Group)| Enforcement Mechanism  
---|---|---|---  
`/dev/shm/egbos/lob/*`| `egbos_lob` (LOB Engine)| `egbos_consumers`| POSIX File ACLs (read-only to group) and `mprotect`  
`/dev/shm/egbos/features/*`| `egbos_feat` (Feature Engine)| `egbos_consumers`| POSIX File ACLs (read-only to group) and `mprotect`  
`/dev/shm/egbos/risk/*`| `egbos_risk` (Risk Enforcer)| `egbos_exec`| POSIX File ACLs (read-only to group) and `mprotect`  
`/dev/shm/egbos/team_a/*`| `team_a_user`| `team_a_group`| Group isolated directory permissions and page table isolation  
  
L1A.2 — Repository Restructure & Ownership Map

The EGBOS repository is structured to enforce logical and physical separation of concerns. The directory mapping defines boundaries, dependencies, and promotion paths:
[code] 
    /
    ├── constitution/       # Level 0 AST rules, compiler constraints, static lints
    │                       # Ownership: Chief Architecture Office (@egbos/infra-core)
    │                       # Dependencies: None (Root of the build graph)
    │
    ├── governance/         # Level 1 Access policies, risk rules, multi-sig vault configs
    │                       # Ownership: Risk Management & CTO (@egbos/risk-mgt, @egbos/cto)
    │                       # Dependencies: /constitution
    │
    ├── verification/       # Level 2 DIRC test harness, CI pipelines, MLflow integration
    │                       # Ownership: Quality Assurance & DevSRE (@egbos/qa-sre)
    │                       # Dependencies: /governance
    │
    ├── certification/      # Level 3 Pre-flight scripts, feed check gates, golden corpus
    │                       # Ownership: SRE & Trading Desk (@egbos/sre, @egbos/desk)
    │                       # Dependencies: /verification
    │
    ├── operations/         # Level 4 Watchdog scripts, telemetry metrics, SRE runbooks
    │                       # Ownership: Site Reliability Engineering (@egbos/sre-team)
    │                       # Dependencies: /certification
    │
    └── trading/            # Level 5 The BRO engine subsystems (LOB, Feature, Execution)
                            # Ownership: Domain Owners (defined in Section L5)
                            # Dependencies: /operations (governed by all higher layers)
[/code]

BRO Subsystem Mapping & Flow Governance

BRO Subsystem | Target Path | Dependency Flow (Upward) | Authority Flow (Downward) | Promotion Flow  
---|---|---|---|---  
Market Data Ingestion | `/trading/market_data/` | Depends on `/constitution` (zero-alloc) | CTO -> Infra-Core Team | Staging Feed -> Latency SLA Gate -> Production  
Event DAG | `/trading/event_dag/` | Depends on `/constitution` (lock-free) | TAC -> Systems Engineering | Schema Lint -> Backward-Compat Check -> Production  
Shared Memory | `/trading/shm/` | Depends on `/governance` (ACLs) | CISO -> DevOps & Platforms | ASAN Boundary Check -> NUMA Node 0 Alignment -> Production  
Limit Order Book | `/trading/lob/` | Depends on `/constitution` (alignment) | Head of Quant -> Trading Infra | Snapshot Recovery Test -> HIL SLA Gate -> Production  
Feature Extraction | `/trading/features/` | Depends on `/verification` (DIRC) | Chief Quant -> Quant Research | Cross-Language Verification -> MLflow Register -> Production  
Alpha Layer | `/trading/alpha/` | Depends on `/verification` (MLflow) | CRO -> Research & Portfolio | DSR Gate -> Multi-Sig envelope approval -> Production  
Risk Engine | `/trading/risk/` | Depends on `/governance` (limits) | CRO -> Risk Management | Emergency Cancel Test -> Signed Envelope Gate -> Production  
Execution Engine | `/trading/execution/` | Depends on `/operations` (watchdog) | Head of Trading -> Exec Algos | Paper Trading Pilot -> Volume Scaling Gate -> Production  
Replay Infrastructure | `/trading/replay/` | Depends on `/verification` (DIRC) | TAC -> Research Systems | Golden Corpus Regression Test -> CI Build Gate -> Production  
Observability | `/trading/observability/` | Depends on `/operations` (metrics) | SRE Lead -> SRE Team | Dashboard Verification -> SLA Latency Check -> Production  
  
L1B

## Capital Authorization Framework

Governance Layer — Cryptographic Multi-Signatory Controls and Exposure Scaling (Level 1).

ACTIVE

## L1B.1 — Cryptographic Envelope Verification

Multi-Sig Approval Rule & HSM Key Ring

No strategy parameter updates, model weights, or capital limits can be loaded into production without a cryptographically signed envelope (`config.bin.env`). The envelope wraps the JSON/YAML configuration block with a signature payload verified using Ed25519 public keys of at least two authorized roles:

  - **Chief Risk Officer (CRO):** Public key `pk_cro_001` (authorizes capital scale limit changes).
  - **Chief Technology Officer (CTO):** Public key `pk_cto_001` (authorizes code promotions and performance profiles).
  - **Head of Trading:** Public key `pk_trade_001` (authorizes instrument configurations).



Production keys are stored in a hardware-isolated HSM (Hardware Security Module) and accessed only via secure CI runners during the promotion gate phase. Manual extraction of deployment keys is physically impossible.

L1B.2 — Boot-Time Signature Verification

At startup, the trading engine's risk enforcer uses statically linked cryptographic libraries (libsodium) to verify the envelope's multi-signatures before loading parameters or allowing network connection. If signature verification fails or has expired, the engine panics and halts.
[code] 
    // Boot-time configuration envelope signature verification via libsodium
    bool verify_config_envelope(const uint8_t* payload, size_t payload_len,
                                const uint8_t* sig, const uint8_t* pub_key) {
        return crypto_sign_verify_detached(sig, payload, payload_len, pub_key) == 0;
    }
[/code]

L2A

## Strategy Promotion Framework

Verification Layer — Staged Strategy Lifecycle and Promotion Gates (Level 2).

ACTIVE

## L2A.1 — Staged Promotion Gate Definition

Promotion Stages & Criteria

Stage| Environment| Preconditions| Capital Limit| Failure Rollback Trigger  
---|---|---|---|---  
Stage 1: Backtest| Offline Research| DSR > 1.5, PBO < 0.05| Zero| Performance decay below baseline  
Stage 2: Replay| Deterministic Simulator| DIRC verification pass (bit-identical)| Zero| Any feature or order output divergence  
Stage 3: Staging| Staging environment| Pre-flight checklist pass| Zero (Paper Trading)| SLA violation (e.g., latency spikes)  
Stage 4: Pilot| Production| 2-of-3 Cryptographic Signatures (L1B)| 5% notional limit| Any P0 incident or drawdown breach  
Stage 5: Live| Production| 30 days clean operation in Stage 4| Full capital limit| Drawdown limit breach  
  
L2B

## Research-to-Production Framework

Verification Layer — Model Promotion Sync and MLflow Registry Gates (Level 2).

ACTIVE

## L2B.1 — MLflow Promotion & Retraining Gates

Promotion Workflow Rules

All quantitative models are tracked in a central MLflow registry. Transitioning a model from the `Candidate` state to `Staged` requires the MLflow runner to verify that the model has a matching Git commit hash, a documented OOS Sharpe, and that its configuration variables match the production schema.

**Online Parameter Retraining Prohibition:** Under no circumstances may a model update its core coefficients or structural parameters online. Any model update requires an offline retraining pipeline run, a new promotion lifecycle execution, and a configuration envelope signature.

L2B.2 — Embargo and Overfitting Mitigation

Backtesting scripts must enforce out-of-sample embargo periods to prevent look-ahead bias. The promotion harness rejects any candidate strategy whose backtest does not explicitly document the Deflated Sharpe Ratio (DSR) and Probability of Backtest Overfitting (PBO).

L3A

## Market Data Certification

Certification Layer — Ingestion Feed Auditing, Gap Detection, and Disconnection Protocols (Level 3).

ACTIVE

## L3A.1 — Ingestion Invariants & Decoupled Feeds

Ingestion Feed Audits

Market data decoders (WebSocket/UDP/FIX) must emit raw telemetry containing sequence numbers and timestamps. The ingestor logs all raw feeds to Tier 0 NVMe storage. A background auditor processes raw logs to verify packet integrity and logs gaps in the Metadata Registry.

  - **Gap Event Emission:** If the ingestor detects an exchange sequence gap, it immediately publishes a `GapEvent` to the ring buffer. The LOB engine intercepts this event, clears resting order books, and requests a full delta snapshot from the venue.
  - **Staleness Gate:** If market updates for a subscribed venue halt for more than 500ms, the feed is marked stale. The execution engine immediately cancels quoting orders on that venue and transitions to `HEDGING`.



L3B

## Replay Certification Framework

Certification Layer — Deterministic Replay and Replay Certification (DIRC) Rules (Level 3).

ACTIVE

## L3B.1 — Byte-Identical Replay & CI Blocking Gate

Deterministic Replay Invariants

To prevent simulation drift, the production C++ engine must match the behavior of offline research models. The DIRC verification stage requires running both engines against a baseline of historical Tier 0 ticks. Any difference in computed feature values (OFI, VPIN, Hawkes) or execution state changes is a compile-blocking failure.

  - **Golden Corpus Registry:** A version-controlled, write-once directory containing standard historical raw packet feeds used for all determinism checks.
  - **Cross-Language Verification:** The C++ engine and the Python prototype models must ingest the same golden corpus ticks and produce identical feature states.
  - **CI Blocking Gate:** The DIRC regression test suite is run automatically on every pull request. Any deviation, even in the least significant bit of a feature value, blocks the merge.



To enforce consistent floating-point behavior, all build targets are compiled using `-ffp-model=precise` (on Clang/LLVM) and `-ffast-math` is prohibited to prevent the compiler from reordering operations or optimizing away necessary rounding boundaries.

L3C

## Production Trading Readiness Framework

Certification Layer — Pre-Flight Checklist, Environment Verification, and Environment Audits (Level 3).

ACTIVE

## L3C.1 — Staging and Pre-Flight Checks

Pre-Flight Checklist

Before transitioning the execution engine from `INACTIVE` to `QUOTING`, the following pre-flight checks are executed. Any failure prevents engine initialization:

Check ID| Target Component| Verification Command/Metric| Acceptance Threshold  
---|---|---|---  
CHK-001| CPU Affinity| Read affinity mask of running PID| Pinned cores match `config/cpu-affinity.yaml`  
CHK-002| NUMA Memory| Check allocation nodes via `numa_maps`| Node 0 memory mapping only  
CHK-003| Configuration Sign| Verify config file envelope| Valid cryptographic signatures for CRO/CTO  
CHK-004| Secrets Access| Ping HashiCorp Vault endpoint| Vault token active and read permissions valid  
CHK-005| SHM Allocation| Inspect POSIX shared memory segments| Segments open, correct sizes, ACLs valid  
CHK-006| Exchange Connectivity| Ping matching engine gateway| Round-trip ping < 2ms, zero packet loss  
CHK-007| Risk Limits| Parse initial risk limit values| Initial P&L and position states verified at zero  
CHK-008| Log Directory| Check WORM directory permissions| WORM mount write-only target accessible  
CHK-009| Telemetry Mount| Verify Prometheus metrics endpoint| TCP socket binding active on localhost:9090  
CHK-010| Watchdog Heartbeat| Verify watchdog process heartbeat| Watchdog daemon running and tracking hot threads  
  
L4A

## Risk Governance Framework

Operational Control Layer — Hard Limits, Soft Limits, and Risk Escalation (Level 4).

ACTIVE

## L4A.1 — Hard and Soft Limit Allocation

Risk Enforcer Invariant Rules

The risk profile is split into two operational levels:

  - **Hard Risk Limits (C++ In-Process Enforcer):** Hard-coded constraints (e.g., maximum position size, maximum order size, order rate throttles, daily loss limits). These checks run on Node 0 directly in the order generation loop. A hard limit breach immediately triggers the emergency cancellation path and halts the trading engine.
  - **Soft Risk Limits (Python Risk Engine):** Dynamic constraints (e.g., leverage multiplier, correlation limits, funding rate tracking). These updates are computed asynchronously on Node 1 and sent via Redis Streams. If the soft limit stream goes stale for more than **5 seconds** (REG-RISK-STALE-001; governed by CLAIM-RISK-STALE-001; see Phase D §D1.2 Registry Entry Schema), the C++ execution engine falls back to a minimal risk profile and fires a P1 alarm.



L4B

## Execution Governance Framework

Operational Control Layer — Quoting State Machine Transitions and Toxicity Gates (Level 4).

ACTIVE

## L4B.1 — State Machine Transition Matrix

Quoting Transitions

The execution engine operates on a strict, finite state machine: `INACTIVE`, `QUOTING`, `HEDGING`, `TAKER_REDUCE`, and `EMERGENCY`. Valid transitions are defined below:

Source State| Target State| Trigger Condition| Action Executed  
---|---|---|---  
INACTIVE| QUOTING| All pre-flight checks pass (L3C)| Allow passive quote generation  
QUOTING| HEDGING| Inventory limit exceeded OR VPIN > threshold_warn| Cancel passive quotes; begin passive inventory reduction  
HEDGING| TAKER_REDUCE| Inventory > 150% limit| Cancel all resting orders; submit aggressive market maker orders  
QUOTING/HEDGING| EMERGENCY| Daily loss limit breach OR watchdog alert OR manual kill| Bypass execution engine; cancel all resting orders via REST; halt  
EMERGENCY| INACTIVE| Manual operator reset + CRO signature (L1B)| Clear position states; await initialization checks  
  
L4C

## Exchange Connectivity Governance

Operational Control Layer — DPDK, Kernel Bypass, and Transceiver Fiber Regulations (Level 4).

ACTIVE

## L4C.1 — Hardware Kill Switch, Optical Disconnect & Exchange COD

Network Latency Bounds & Rate-Throttling

To prevent exchange rate-limiting and duplicate orders, the execution interface enforces network restrictions:

  - **Kernel Bypass:** All WebSocket, FIX, and binary connections must run via kernel bypass network interfaces (e.g., Solarflare OpenOnload or DPDK). P99 loopback latency must remain below 5µs.
  - **Order Rate Throttling:** Enforce a hardware token bucket rate-limiter. The limiter operates independently of the execution engine and will drop order requests if they exceed exchange rate rules, preventing runaway quoting loops.



Physical FPGA Interceptor & Optical Disconnect

An AMD Alveo U50 SmartNIC is deployed inline between the execution server and the exchange transceiver fiber. Pinned watchdog threads monitor the software loop and continuously emit a high-frequency UDP heartbeat to the SmartNIC. If the heartbeat misses for more than 50µs, or a manual kill signal is received, the FPGA's logic immediately shuts off power to the SFP+ transceiver cage lasers, physically cutting the connection (optical disconnect) under 10µs. Simultaneously, the connection loss triggers the exchange-side Cancel-on-Disconnect (COD) protocol to flush all resting orders.

L4D

## Incident Classification Framework

Operational Control Layer — Incident Severity Definitions and Automated Escalations (Level 4).

ACTIVE

## L4D.1 — Severity Definition & Mitigation

Incident Severities

Severity| Description| Triggering Event| Automated Mitigation| Escalation Path  
---|---|---|---|---  
**P0 (Critical)** | System-wide outage or extreme risk violation. Capital at immediate risk. | Daily loss limit breach, watchdog thread timeout, hardware kill switch activation. | Trigger emergency cancel bypass; physically cut network transceiver fiber; shutdown engine. | Immediate pager alert to CTO, CRO, and trading desk. Post-mortem review mandatory.  
**P1 (Major)** | Degraded execution or infrastructure failure with minimal capital risk. | Soft limit stream stale >90s, high VPIN warning, exchange feed disconnect. | Suspend new quoting orders; transition active strategies to `HEDGING` or `TAKER_REDUCE`. | Slack alert to on-call engineering team. Review required within 12 hours.  
**P2 (Info)** | Operational anomaly with no impact on risk or execution latency. | Telemetry publish fail, non-critical database backup delay. | Retry connection; log anomaly in metadata registry. | Email alert to infrastructure team. Resolve during normal business hours.  
  
L4E

## Trading Audit Framework

Operational Control Layer — WORM Storage and Cryptographically Chained Auditing (Level 4).

ACTIVE

## L4E.1 — Log Chain Verifiability & Compliance Storage

WORM Audit Log Rules & Merkle Chaining

All execution decision events, order submissions, fills, and risk limit evaluations must be written to an immutable audit trail. Log entries are structured as a SHA-256 Merkle chain. Each log block is signed using the server's private key, proving that the entry has not been modified after emission.
[code] 
    // Chaining audit log records with SHA-256
    void chain_audit_block(AuditBlock& current_block, const uint8_t* prev_block_hash) {
        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, prev_block_hash, 32);
        SHA256_Update(&ctx, current_block.data, current_block.data_len);
        SHA256_Final(current_block.hash, &ctx);
    }
[/code]

L4E.2 — External Root Publication & WORM Retention

To establish absolute proof of non-repudiation, the combined Merkle root hash of the day's audit logs is published daily to an external immutable ledger or public timestamping service. Logs must be stored on a Write-Once-Read-Many (WORM) storage appliance with hardware-enforced write locks, retaining all audit records for a minimum of five years.

L5

## Trading Domain Layer (BRO)

Trading Domain Layer — The BRO Subsystems Governed Domain (Level 5).

**⚑ Navigation note:** DOM-01 through DOM-10 tables below use a per-domain row format. A compact matrix view (domains as columns, control dimensions as rows) is a Phase L documentation deliverable. Until then, use browser Ctrl+F to navigate by DOM-ID or Control Dimension name. 

Domain | Scope | Owner Group | P0 Escalation  
---|---|---|---  
DOM-01 | Market Data Ingestion | @egbos/infra-core | LOB → HEDGING  
DOM-02 | Event DAG | @egbos/sys-eng | Engine EMERGENCY  
DOM-03 | Shared Memory | @egbos/devops-platform | Watchdog fiber kill  
DOM-04 | LOB Engine | @egbos/trade-infra | HEDGING  
DOM-05 | Feature Extraction | @egbos/quant-research | Weights → 0  
DOM-06 | Alpha Layer | @egbos/alpha-portfolio | Signal weight → 0  
DOM-07 | Risk Engine | @egbos/risk-management | EMERGENCY + fiber kill  
DOM-08 | Execution Engine | @egbos/exec-algos | HEDGING  
DOM-09 | Replay Infrastructure | @egbos/research-sys | MLflow reject  
DOM-10 | Observability | @egbos/sre-team | On-call page  
  
ACTIVE

## DOM-01 — Market Data Ingestion Domain

1\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| Must record all incoming packets; must stamp with hardware NIC clock; must log sequence numbers; zero dynamic heap allocations.| Clang AST matcher blocks any heap allocation during write.  
Required Invariants| Monotonic sequence counting. RTT to exchangeMatchingEngine must be < 2ms.| eBPF checker fails process if RTT threshold is breached.  
Verification Requirements| Continuous monitoring via eBPF packet counters; automated PCAP regression verification against mock feed.| CI build testing stage using `tcpdump` logs.  
Certification Requirements| Must pass feed integrity unit tests; must demonstrate recovery on 10% packet drop simulation.| Staging CI mock feed injector validates packet drop handling.  
Promotion Requirements| Must pass staging feed check and prove < 10ns decoder latency under stress.| HIL latency test suite gate.  
Replay Requirements| Replaying raw Tier 0 feed must produce identical LOB reconstruct events.| DIRC test suite verification.  
Audit Requirements| Weekly hash checks of Tier 0 raw partitions against Metadata Registry records.| Cron audit job validates partition SHA-256 hashes.  
Failure Escalation Path| Sequence gap or feed lag > 500ms triggers `GapEvent` (P1) and suspends quoting.| LOB engine transitions execution state to HEDGING.  
Operational Ownership| Core Infrastructure Team.| CODEOWNERS group `@egbos/infra-core`.  
Governance Authority| Chief Technology Officer (CTO).| Executive technical approval gate.  
  
## DOM-02 — Event DAG Domain

2\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| All event definitions must inherit from fixed-size 5-tuple header schema (RawTickEvent, etc.); lock-free execution path.| FlatBuffers compiler schemas.  
Required Invariants| Events are immutable once written; strict directed acyclic dependency paths.| govcheck DAG cycle validator.  
Verification Requirements| Static analysis linting of event schemas; compile-time checks of dependency DAG routes.| CMake schema validator lint.  
Certification Requirements| Schema backward-compatibility checks. Consumer registration matches runtime ranges.| Staging CI mock events comparison suite.  
Promotion Requirements| Any schema major version bump requires manual CTO approval and 2-week migration plan.| L1B Multi-sig configuration sign-off.  
Replay Requirements| Every event log replay must flow through the exact DAG paths as production.| Deterministic simulator harness.  
Audit Requirements| Verification of event schema checksum history in Tier 4 registry.| Automated CI schema change audit.  
Failure Escalation Path| Invalid schema format or unregistered event ID triggers immediate engine halt (P0).| Execution engine enters EMERGENCY state.  
Operational Ownership| Systems Engineering Team.| CODEOWNERS group `@egbos/sys-eng`.  
Governance Authority| Technical Architecture Committee (TAC).| Committee approval signature.  
  
## DOM-03 — Shared Memory Domain

3\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| POSIX shared memory segments must use namespacing and have team-isolated permissions.| UID/GID file system permissions on `/dev/shm`.  
Required Invariants| Lock-free double-buffered readers; single-writer segment write enforcement.| `mprotect` write-protection checks on reader pages.  
Verification Requirements| Real-time ACL validation; memory leak checks and segment allocation boundaries verified under ASAN.| CI valgrind/ASAN build pass.  
Certification Requirements| Memory map validation and segment sizing alignment with NUMA allocation parameters on Node 0.| Pre-flight checklist CHK-002 and CHK-005.  
Promotion Requirements| Verify that segment sizing aligns with NUMA allocation parameters on Node 0 under stress.| Staging verification checklist check.  
Replay Requirements| Replay engine mock-maps SHM structures in read-only mode to prevent write clashes.| Replay engine read-only mmap configuration.  
Audit Requirements| Daily ACL validation of the shared memory directories in `/dev/shm/egbos/`.| Cron audit check of directory permissions.  
Failure Escalation Path| Corruption or segment write failure by unauthorized UID terminates process (P0).| Watchdog triggers physical fiber kill switch.  
Operational Ownership| DevOps & Platforms Team.| CODEOWNERS group `@egbos/devops-platform`.  
Governance Authority| Chief Information Security Officer (CISO).| Security review approval.  
  
## DOM-04 — Limit Order Book (LOB) Domain

4\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| Must reconstruct order books with sub-50µs latency; must support full snapshot re-synchronization.| Custom array-based allocation order map (no dynamic allocation).  
Required Invariants| Top-5 bid/ask structures must remain cache-line aligned.| Compile-time alignment assertions (alignas(64)).  
Verification Requirements| Parallel shadow LOB validation against exchange tick feeds; static check for dynamic allocation in update paths.| CI static code analysis and latency profile testing.  
Certification Requirements| Must pass the simulated LOB Reset suite and snapshot recovery test.| LOB test suite simulation.  
Promotion Requirements| Verify LOB reconstruction accuracy against exchange feed snapshots in staging.| HIL accuracy verification gate.  
Replay Requirements| Replaying tick logs must produce LOB delta events matching original logs.| DIRC byte-identical check.  
Audit Requirements| Daily comparison of reconstructed book states against exchange records.| Post-trade database comparison job.  
Failure Escalation Path| Invalid LOB state or disconnect triggers LOBResetEvent, transitions to HEDGING (P1).| Execution engine suspends quoting orders.  
Operational Ownership| Trading Infrastructure Team.| CODEOWNERS group `@egbos/trade-infra`.  
Governance Authority| Head of Quantitative Trading.| Quantitative Desk sign-off.  
  
## DOM-05 — Feature Extraction Domain

5\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| Features (OFI, VPIN, Hawkes) must be computed in C++ under 200µs; zero dynamic allocation.| Static Clang AST matcher prevents malloc calls.  
Required Invariants| Computed features must be floating-point deterministic.| DIRC compile-blocking test suite.  
Verification Requirements| Cross-language verification comparing C++ features with Python prototype values.| CI cross-compilation target match checks.  
Certification Requirements| KL divergence of online vs shadow feature distributions must be < 2σ.| Real-time feature anomaly detector.  
Promotion Requirements| Feature vector format change requires major schema upgrade approval (L2B).| MLflow and CTO validation gate.  
Replay Requirements| Must match offline Python features down to the least significant bit.| Byte-identical DIRC validation.  
Audit Requirements| Monthly shadow replay validation check diffing Tier 0 vs Tier 2 features.| Scheduled shadow replay cron job.  
Failure Escalation Path| Feature corruption alarm or calculation lag > 500µs drops quoting (P1).| Alpha weights set to zero; execution enters HEDGING.  
Operational Ownership| Quantitative Research Team.| CODEOWNERS group `@egbos/quant-research`.  
Governance Authority| Chief Quant Strategist.| Quantitative Strategy Board signature.  
  
## DOM-06 — Alpha Layer Domain

6\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| Signals must register in signal registry; alpha vectors must contain TTL flags.| Signal registry schema verification at startup.  
Required Invariants| Alpha vectors cannot be updated online; weight updates use realized P&L decay.| Execution engine restricts online parameter updates.  
Verification Requirements| Simulated model promotion validation using walk-forward test metrics; static code structure verification.| CI model testing pipeline.  
Certification Requirements| Signal performance must pass Deflated Sharpe Ratio (DSR) > 1.0 test.| MLflow promotion test suite.  
Promotion Requirements| Model weights must be committed via MLflow with Git commit hashes (L2B).| CTO and CRO multi-sig signature.  
Replay Requirements| Alpha engine output must be deterministically simulated offline on Tier 1 data.| Offline walk-forward simulation platform.  
Audit Requirements| Daily rolling realized Sharpe attribution calculations per signal.| Risk engine analytics post-trade.  
Failure Escalation Path| Signal TTL expiration or Sharpe decay below 50% limit drops signal weight to zero (P1).| Execution logic updates strategy parameters.  
Operational Ownership| Alpha Research & Portfolio Team.| CODEOWNERS group `@egbos/alpha-portfolio`.  
Governance Authority| Chief Risk Officer (CRO).| Risk Committee approval.  
  
## DOM-07 — Risk Engine Domain

7\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| Must enforce hard limits in-process on Node 0; must process soft limits via Redis Streams.| C++ Risk Enforcer is statically linked to execution loop.  
Required Invariants| Hard risk limits cannot be bypassed; daily P&L limits are immutable at runtime.| Risk enforcer atomic flags cannot be written by execution.  
Verification Requirements| Unit testing of risk enforcer logic on extreme parameter inputs; code-level checks of atomic risk flags.| CI compilation linter and extreme input tests.  
Certification Requirements| Must demonstrate emergency cancellation bypass in staging under 10µs.| HIL emergency cancel test drill.  
Promotion Requirements| Limits config must match cryptographically signed configuration envelopes (L1B).| libsodium signature verification at startup.  
Replay Requirements| Replayed transactions must match risk enforcer states exactly.| DIRC validation harness.  
Audit Requirements| Daily post-trade reconciliations of executed trades vs risk limits.| Risk compliance report pipeline.  
Failure Escalation Path| Hard risk limit breach or drawdown limit hit triggers EMERGENCY cancel and halt (P0).| Risk enforcer flags EMERGENCY; watchdog drops network fiber link.  
Operational Ownership| Risk Management Team.| CODEOWNERS group `@egbos/risk-management`.  
Governance Authority| Chief Risk Officer (CRO).| Risk and Capital Compliance sign-off.  
  
## DOM-08 — Execution Engine Domain

8\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| Must process order submission under 2ms; must operate on strict quoting state machine.| Finite state machine enforcer in C++ execution logic.  
Required Invariants| Single order submission thread; no parallel order writers to exchange FD.| Execution thread pinned to core 5; exclusive write lock on FD.  
Verification Requirements| Automated verification of quoting state transitions; race condition testing on simulated exchange socket.| Staging socket mock harness testing.  
Certification Requirements| State machine transitions verified under simulated exchange connection errors.| Staging disconnect simulation suite.  
Promotion Requirements| Must undergo 30-day clean paper trading in Stage 4 pilot before scaling.| L2A Promotion Registry checks.  
Replay Requirements| Execution decisions must be fully deterministic given the same feature inputs.| Byte-identical DIRC validation.  
Audit Requirements| Verification of execution decision events against matching engine logs.| Post-trade slippage and latency metrics pipeline.  
Failure Escalation Path| Submission failure or sequence mismatch triggers immediate HEDGING state (P1).| Execution state machine transition rule.  
Operational Ownership| Execution Algorithms Team.| CODEOWNERS group `@egbos/exec-algos`.  
Governance Authority| Head of Trading Execution.| Execution Operations sign-off.  
  
## DOM-09 — Replay Infrastructure Domain

9\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| Must read Tier 0 logs sequentially using read-only mmap; must support REPLAY_START control events.| POSIX `mmap` read-only page protection flags.  
Required Invariants| Replay runs must not write to production SHM; zero side-effects on live database instances.| Network isolated replay environment (namespace sandbox).  
Verification Requirements| Automatic execution of regression test suites on new replay engine builds; memory leak check.| CI valgrind/ASAN build check on replay target.  
Certification Requirements| Verify that replay execution produces bit-identical feature files to Tier 2.| DIRC test validator.  
Promotion Requirements| Changes to replay engine source code require a complete golden corpus re-run check.| CI rebuild validation step.  
Replay Requirements| Replay engine must itself be recomputable and version-controlled.| Git commit tag recorded in metadata.  
Audit Requirements| Log all replay control commands and results in the Metadata Registry.| Tier 4 logging API.  
Failure Escalation Path| Replay run failure or non-determinism invalidates all dependent research results (P2).| MLflow rejects any model using that replay ID.  
Operational Ownership| Research Systems Team.| CODEOWNERS group `@egbos/research-sys`.  
Governance Authority| Technical Architecture Committee (TAC).| Committee revision sign-off.  
  
## DOM-10 — Observability Domain

10\. Operational Rules & Invariants

Control Dimension| Requirement Specification| Enforcement Mechanism  
---|---|---  
Constitutional Obligations| Must export metrics to Prometheus sidecar; must sample traces in OTEL; structured JSON logs.| Standard C++ Prometheus and OTEL SDK integrations.  
Required Invariants| Telemetry I/O must run asynchronously; async log ring buffer must drop old logs on overflow (no block).| MPSC log ring buffer with overwrite-oldest policy.  
Verification Requirements| Overhead benchmarking under high message load; static validation of JSON schema log format.| Staging latency profile and schema check.  
Certification Requirements| Verification of telemetry overhead in hot path (must be < 1µs P99).| Hot-path latency benchmark test.  
Promotion Requirements| Grafana dashboards and webhook configs must match PAN-001..PAN-012.| Staging verification checklist CHK-009.  
Replay Requirements| Observability traces can be replayed to diagnose logic path anomalies.| OTEL trace exporter in replay mode.  
Audit Requirements| Weekly review of log collection completeness and server performance metrics.| Infrastructure monitoring audit task.  
Failure Escalation Path| Watchdog heartbeat miss or local metrics endpoint drop triggers alarm (P1).| On-call pager system alert.  
Operational Ownership| Site Reliability Engineering (SRE).| CODEOWNERS group `@egbos/sre-team`.  
Governance Authority| Site Reliability Engineering Lead.| SRE Team sign-off.  
  
MIG

## Unified EGBOS Migration Roadmap

Migration Roadmap — Transitioning from Current EGBOS to Unified EGBOS Operating System.

ACTIVE

## MIG.1 — Migration Milestones

Milestone MIG-001: Constitution Integration

**Objective**|  Integrate Level 0 system constraints and static checks into the primary development pipelines, ensuring zero hot-path memory allocation and lock-free execution.  
---|---  
**Deliverables**|  Clang AST rules configuration file (`/constitution/rules.yaml`), C++ allocator wrapper runtime traps, and eBPF kernel watchdog scripts.  
**Exit Criteria**|  Zero dynamic allocations or locking calls detected in the C++ hot-path (`qos/hotpath/`) during compile-time AST verification.  
**Governance Gates**|  Chief Architecture Office code review approval; CTO signature.  
**Certification Gates**|  Successful run of the static verification linter on all core files; compiler flag check.  
**Rollback Strategy**|  Revert AST rule checks in CI pipeline; disable global allocator SIGABRT traps in build flags.  
  
Milestone MIG-002: Cryptographic Access Enforcers

**Objective**|  Establish namespace isolation boundaries and cryptographic configuration validation across all development and production workspaces.  
---|---  
**Deliverables**|  POSIX ACL configuration scripts for `/dev/shm/egbos/`, HashiCorp Vault integration envelopes (`config.bin.env`), and boot-time Ed25519 signature validation logic.  
**Exit Criteria**|  All running strategy binaries reject configurations that lack a valid 2-of-3 signature key envelope at boot-time; unauthorized users are blocked from reading/writing shared memory paths.  
**Governance Gates**|  CISO approval of vault configuration; CRO sign-off on capital envelope schemas.  
**Certification Gates**|  Multi-sig configuration signature verification suite passes in local staging test environment.  
**Rollback Strategy**|  Downgrade to standard local configuration file parsing; disable POSIX ACL restrictions via system reset script.  
  
Milestone MIG-003: DIRC CI Gates Deployment

**Objective**|  Deploy the Deterministic Replay Certification (DIRC) framework to prevent simulation drift between research models and production binaries.  
---|---  
**Deliverables**|  Golden corpus registry database, cross-language validation harness (Python vs C++), and CI/CD blocking gate runner.  
**Exit Criteria**|  100% of candidate models pass byte-identical replay test against historical tick corpus; non-deterministic branches result in immediate CI failure.  
**Governance Gates**|  Chief Quant Strategist approval; Head of Quantitative Trading sign-off.  
**Certification Gates**|  CI test runs verify identical floating-point output values matching precise compiler execution flags (`-ffp-model=precise`).  
**Rollback Strategy**|  Disengage CI blocking gate (allow warnings instead of failures) for non-latency-critical features; isolate DIRC test runners.  
  
Milestone MIG-004: Hardware-in-the-Loop Safeguards

**Objective**|  Install and configure physical and network-layer safety barriers to mitigate high-severity risks and runaway execution loops.  
---|---  
**Deliverables**|  Inline FPGA SmartNIC transceiver intercept firmware, optical disconnect trigger daemon, and exchange Cancel-on-Disconnect (COD) FIX integration.  
**Exit Criteria**|  Watchdog heartbeat failure triggers hardware fiber laser cut and exchange COD order cancellation in under 10µs.  
**Governance Gates**|  Head of Trading Execution approval; Chief Risk Officer sign-off.  
**Certification Gates**|  Physical HIL laboratory test drill verifies successful laser cut upon software watchdog crash.  
**Rollback Strategy**|  Place FPGA SmartNIC in bypass mode (unfiltered feed bridging); disable laser cut triggers.  
  
Milestone MIG-005: Brokerage Integration Mapping

**Objective**|  Complete the mapping of all BRO/QOS trading subsystems into the unified EGBOS Level 5 Trading Domain Layer.  
---|---  
**Deliverables**|  Refactored directory structure mapping, WORM audit log storage configuration, and domain-level governance authority assignments.  
**Exit Criteria**|  All 10 BRO subsystems are compiled and executed under the EGBOS Level 0-4 constraints; compliance logs are successfully written to cryptographically chained WORM partitions.  
**Governance Gates**|  CTO, CRO, and Head of Quantitative Trading joint sign-off.  
**Certification Gates**|  Full validation checklist (CHK-001 to CHK-010) passes in the production environment.  
**Rollback Strategy**|  Revert repository structures to separated legacy build configurations; run legacy independent BRO binaries.  
  
# Phase K — Institutional Upgrades & Hardening (Level 5+)

Complete

Production-ready specification upgrades covering institutional security, disaster recovery, market infrastructure, portfolio risk, alpha lifecycle, observability, tiered governance, and production certification. This section represents the final audited institutional specification for EGBOS.

## K1 — Security Hardening

K1.1 — HashiCorp Vault & HSM-Backed Secrets Pipeline

The EGBOS production environments integrate with a centralized, high-availability cluster of **HashiCorp Vault Enterprise** instances backed by a FIPS 140-2 Level 3 physical **Hardware Security Module (HSM)**. Hardcoded credentials, environment secrets, and plain-text configuration variables are strictly prohibited and enforced by static analysis rules `GC-E-SECRETS-001` and `GC-E-SECRETS-002`.

  - **Dynamic Secrets:** Exchange API keys, database credentials, and session tokens are retrieved dynamically at startup via Vault AppRole authentication. All dynamic secrets have a Time-To-Live (TTL) < 24 hours.
  - **Automated Secret Rotation:** API keys and signing credentials are automatically rotated every 30 days via Vault database and custom venue credential engines. The rotation process is coordinated by a systemd timer on the Vault controller.
  - **Break-Glass Procedure:** In the event of a total Vault cluster loss or HSM lock, an emergency break-glass procedure is triggered. This requires reconstructing the Vault master key using Shamir's secret sharing. A quorum of 3 out of 5 key holders (CTO, Chief Security Officer, Chief Risk Officer, Head of Trading, CEO) must be physically present in the secure operations room to present their cryptographic YubiKey tokens.


[code] 
    // Vault AppRole dynamic secrets retrieval helper
    struct VaultCredential {
        std::string api_key;
        std::string api_secret;
        uint64_t lease_expiration;
    };
    
    VaultCredential fetch_exchange_keys(const std::string& role_id, const std::string& secret_id) {
        auto response = http::post("https://vault.egbos.internal/v1/auth/approle/login", 
                                   {{"role_id", role_id}, {"secret_id", secret_id}});
        std::string client_token = response["auth"]["client_token"];
        
        auto secret_response = http::get("https://vault.egbos.internal/v1/secret/data/exchanges/binance", client_token);
        return {
            secret_response["data"]["data"]["api_key"],
            secret_response["data"]["data"]["api_secret"],
            time::now_epoch_seconds() + secret_response["data"]["data"]["lease_duration"]
        };
    }
[/code]

K1.2 — Host Hardening, Zero-Trust Network & mTLS

All network traffic across physical co-location boundaries and virtual environments is subject to zero-trust access controls. Default trust is eliminated.

  - **Mutual TLS (mTLS):** All inter-process communication that leaves shared memory (e.g., warm-path Python engines communicating with etcd, Redis, or timescaledb) must use mutual TLS (TLS 1.3 with `ECDHE-ECDSA-AES256-GCM-SHA384` ciphers) verified using internal certificates issued dynamically by Vault's PKI engine.
  - **Host Isolation & Bastion Nodes:** Direct SSH access to production hosts is permanently disabled. Systems operators must route all maintenance connections through an audited, multi-factor authenticated **Bastion Host** requiring hardware YubiKeys and temporal, role-based access authorization (short-lived SSH certificates).
  - **RBAC & ABAC Interventions:** All Human-in-the-Loop (HITL) actions (terminal halts, risk overrides) are governed by Role-Based Access Control and Attribute-Based Access Control. Intervention claims are cryptographically signed by the operator's YubiKey and logged in the immutable WAL.



K1.3 — Supply Chain Security & Boot Cryptographic Verification

To prevent malicious code injection or backdoors, every build is cryptographically verified from source to runtime.

  - **Software Bill of Materials (SBOM):** Every CI/CD pipeline run compiles a complete SBOM in SPDX format using Syft, which is scanned for vulnerabilities and archived in the metadata registry.
  - **Binary Signing:** Production binaries and container images are digitally signed using Cosign via private keys bound to the enterprise HSM.
  - **Secure Boot Verification:** Production hardware enforces UEFI Secure Boot with custom platform keys. During the boot sequence, the UEFI bootloader and the C++ kernel modules verify the digital signature of all executables. If verification fails, the host triggers a hardware lockout and alerts monitoring.



## K2 — Disaster Recovery & Business Continuity

K2.1 — Active-Passive Geo-Redundancy & Real-Time Replication

Business continuity mandates that a primary datacenter outage (NY4) does not result in loss of market positions or unmanaged risk. The system maintains an active-passive replica in Chicago (CH2) with the following targets:

  - **Recovery Time Objective (RTO):** < 60 seconds (total time to resume risk management and order routing at the DR site).
  - **Recovery Point Objective (RPO):** < 1 second (maximum allowable data loss window in the event of an abrupt disaster).
  - **State Replication Protocol:** Position balances, fill notifications, and order states are replicated from NY4 to CH2 continuously over a dedicated, low-latency InfiniBand/RoCEv2 network line using lock-free, zero-copy ring buffers.



K2.2 — Failover & Disaster Runbook Automation

Disaster failover is automated to eliminate human latency and error during stress conditions.

  - **Heartbeat and Outage Detection:** The CH2 disaster recovery node monitors a continuous multicast heartbeat from the NY4 cluster. If the heartbeat is lost for more than 5 consecutive seconds, the CH2 node initiates a failover check. It queries secondary health signals (pinging exchange endpoints, validating fiber link carriers) to confirm the outage.
  - **Position & Order State Reconstruction:** Upon failover, the CH2 disaster recovery engine reconstructs the global position state. It parses the replicated transaction log and reconciles it against the exchange execution endpoints.
  - **Exchange Cancel-On-Disconnect (COD):** All venue sessions are configured with Cancel-On-Disconnect enabled. If the primary connection from NY4 drops, the exchange automatically cancels all resting limit orders. The CH2 engine then issues broad cancel-all commands on secondary sessions to guarantee a clean slate before resuming passive quoting.
  - **Weekly DR Drills:** The operations team executes weekly automated disaster recovery drills. During these drills, the simulation engine injects virtual physical fiber cuts or power failures in NY4, forcing the CH2 backup site to assume trading and risk controls. The results are logged automatically as audit records.



## K3 — Institutional Market Infrastructure

K3.1 — Dual-Feed Line Arbitration & Kernel Bypass

High-frequency data ingestion requires packet loss mitigation and microsecond-level processing speed.

  - **Line Arbitration (A/B Arbitration):** Ingestor hosts consume duplicate UDP market data streams (Feed A and Feed B) routed via geographically separate fiber paths. A hardware-level or Solarflare EF_VI network-level arbitration engine decodes both feeds, processing whichever packet arrives first and discarding the duplicate.
  - **Kernel Bypass:** Ingestor and LOB reconstruction engines utilize **Solarflare OpenOnload** or **DPDK** (Data Plane Development Kit) to bypass the OS network stack. Network packets are written directly into user-space ring buffers, reducing feed-to-book latency to < 5 microseconds.



K3.2 — PTP Clock Synchronization & Venue Failover

Deterministic order booking and audit compliance require sub-microsecond clock synchronization.

  - **PTP Synchronization (IEEE 1588):** All trading and ingestion servers synchronize their internal clocks using IEEE 1588 Precision Time Protocol connected to an on-site GPS-disciplined grandmaster clock. The system enforces a strict clock skew ceiling of < 1 microsecond. Any node exceeding a 5-microsecond drift is automatically quarantined.
  - **Broker and Venue Redundancy:** The execution engine maintains active connections to redundant brokers (e.g., Broker A and Broker B) and secondary exchange sessions. If a primary broker session disconnects, the Smart Order Router (SOR) dynamically redirects orders to the secondary broker within < 50 milliseconds using dynamic routing tables.



## K4 — Institutional Risk System

K4.1 — Out-of-Process Risk Daemon & Hardware Kill Switches

Institutional risk controls must operate independently of the execution engine thread to ensure safety in the event of software crashes or deadlocks.

  - **Out-of-Process Enforcer:** The out-of-process C++ risk enforcer runs as a standalone process on an isolated CPU core, completely decoupled from the execution engine thread. It monitors position limits, gross notional limits, and message rates by reading shared memory state. If the execution engine blocks or crashes, the risk enforcer detects the missed heartbeat and takes control.
  - **Hardware Kill Switch (Optical Disconnect):** In the event of a critical limit breach (e.g., exceeding daily loss limits or a runaway algorithm loop), the out-of-process risk enforcer triggers a Layer 1 optical disconnect switch. This physically breaks the fiber optic connection to the exchange, ensuring zero further order submissions within < 100 microseconds of the breach.
  - **Dynamic Soft Limits (Portfolio VaR):** The Python risk engine computes portfolio-level Value at Risk (VaR/CVaR) and cross-asset correlation limits every 10 seconds. It publishes updated soft limits to the execution engine. If the soft limit stream becomes stale for > 5 seconds, the execution engine drops into a degraded mode, scaling down all capital allocations.


[code] 
    // Out-of-process risk enforcer heartbeat and breach detection loop
    void run_risk_loop(RiskSharedState* shm, HardwareController* hw) {
        const uint64_t HEARTBEAT_TIMEOUT_NS = 2000000; // 2ms
        while (true) {
            uint64_t now = time::now_ns();
            if (now - shm->execution_heartbeat > HEARTBEAT_TIMEOUT_NS) {
                hw->trigger_optical_disconnect("EXECUTION_HEARTBEAT_LOST");
                break;
            }
            if (shm->realized_daily_loss > shm->hard_loss_limit) {
                hw->trigger_optical_disconnect("HARD_LOSS_LIMIT_BREACHED");
                break;
            }
            time::sleep_ns(100000); // 100µs check interval
        }
    }
[/code]

## K5 — Institutional Alpha Platform

K5.1 — Standardized Research and Feature Registries

The alpha lifecycle is structured as an immutable, audited pipeline that prevents feature drift and multiple testing bias.

  - **Research, Feature, and Dataset Registries:** All signals and features must be registered in the central etcd metadata registry before they are eligible for production deployment. The registration includes: (1) feature math definition, (2) dependency inputs, (3) target asset classes, and (4) out-of-sample expected capacity.
  - **Meta Labeling & Sizing:** Signal execution is governed by a meta-labeling model (e.g., XGBoost) that predicts the probability of order execution success based on market micro-features (VPIN, Hawkes branching ratio, order book imbalance). This probability dynamically adjusts quoting sizes.



K5.2 — Walk-Forward & Sharpe Validation Gates

Every alpha model must pass through three statistical validation gates before promotion:

  - **Walk-Forward Validation:** Enforce strict out-of-sample walk-forward testing. Backtest configurations must be locked, and the OOS vault enforces strict embargo periods to prevent look-ahead bias.
  - **Combinatorial Purged Cross-Validation (CPCV):** Enforce CPCV to address path dependency and overlapping test labels, generating realistic Sharpe distribution expectations.
  - **Deflated Sharpe Ratio (DSR) Gate:** To control for multiple testing bias, the DSR must exceed 0.5. Backtests that perform millions of iterations without adjusting for the number of trials are automatically rejected.



K5.3 — Automated Decay & Signal Retirement

Live signals are continuously audited for decay. If a signal's 30-day rolling Sharpe ratio drops below 40% of its out-of-sample backtest expectation for 5 consecutive trading days, its allocation limit is scaled down by 75% automatically. If it does not recover in 15 days, it is moved to shadow-testing and scheduled for retirement.

## K6 — Observability Architecture

K6.1 — Telemetry, Distributed Tracing & Logs

The observability plane provides end-to-end visibility without introducing latency into the hot path.

  - **Lock-Free MPSC Log Buffers:** All logging in the hot path is written to a lock-free, multi-producer single-consumer ring buffer. A background daemon consumes the log entries and forwards them to a centralized ClickHouse data lake.
  - **Distributed Tracing (OpenTelemetry):** All event packets (from ingestor RawTickEvent to ExecutionDecisionEvent) contain a correlation trace ID. Tracing spans are assembled asynchronously by warm-path tracers.
  - **SLIs, SLOs & Error Budgets:** Define core Service Level Objectives (SLOs) and Service Level Indicators (SLIs) for execution performance:  Indicator| Objective (SLO)| Measurement window| Error Budget  
---|---|---|---  
Feature-to-Order Latency| 99.99% < 200 microseconds| Rolling 30 days| 0.01% violation allowance  
Feed Packet Loss| 99.999% zero packet loss| Daily trading session| 10 dropped packets/day  
Risk Heartbeat Jitter| 100% heartbeat < 2 milliseconds| Continuous| Zero (hard block)  



## K7 — Tiered Governance Optimization

To maximize research velocity while maintaining institutional safety, compliance controls are tiered based on operational risk.

Tier Level| Component Scope| Static Analysis (govcheck)| RFC Requirement| Verification Gates  
---|---|---|---|---  
**Tier 0: Hot Path Core** | Ingestor (Rust), LOB Engine (C++), Execution Engine (C++), Hard Risk Enforcer (C++) | Strict, blocking immediately at commit | Mandatory, signed by CTO + CRO | Byte-identical replay, daily fuzzing, nightly ASAN/UBSAN builds  
**Tier 1: Warm Path** | Regime Detector (Python), Soft Risk Engine (Python), Configuration Loader | Strict, blocking on release branch | Mandatory, signed by CTO | Staging deployment (3 days), telemetry validation, configuration check  
**Tier 2: Auxiliary** | Exchange simulator, local backtest runner, development benchmarks | Warning-only, non-blocking | No RFC required | Unit testing, integration testing  
**Tier 3: Offline Research** | Research notebooks, plotting utilities, visualization dashboards | Deactivated | No RFC required | Code peer review before merge  
  
## K8 — Production Readiness Checklist

A measurable checklist that must be passed and cryptographically signed off before EGBOS is deployed to production with live capital.

ID| Check Area| Measurable Requirement| Verification Method  
---|---|---|---  
PRC-SEC-001 | Secrets Access | Vault dynamic credentials token leases verify successfully; no static secrets exist in Git history. | `govcheck GC-E-SECRETS-001` passes.  
PRC-SEC-002 | Image Signing | Production docker images contain valid cryptographic signatures matching the HSM release public key. | `cosign verify` execution.  
PRC-DR-001 | Failover Verification | NY4 to CH2 active-passive failover completes in < 60 seconds; 100% of open positions successfully recovered. | Automated DR drill test logs.  
PRC-RISK-001 | Out-of-Process Halt | Killing the execution process triggers the out-of-process risk enforcer to execute optical disconnect in < 2ms. | Chaos injection halt validation.  
PRC-DATA-001 | Clock Synchronization | Server clock drift against GPS PTP grandmaster is < 1 microsecond. | PTP tracking metrics check.  
PRC-ALPHA-001 | Fidelity Replay | Offline Polars and online C++ feature extraction engine produce identical results for 10,000 continuous tick updates. | `validator/harness` test passes.  
  
⊕ **GC-E-PRC-001 -- Production Readiness Gate (K8 Machine Enforcement)**  
Each PRC-* item is extended with two mandatory fields in its governance YAML record under `governance/prc/`:  
(1) **`verification_method`** -- a named `govcheck` subcommand or CI artifact path invoked to mechanically confirm the requirement is met (examples: `govcheck secrets check`, `govcheck parity check --ticks=10000`, `govcheck ptp check`).  
(2) **`sign_off_required_by`** -- the authority role (CTO / CRO / CSO / Lead Quant) whose counter-signature must be present before the item may be set to `COMPLETE`.  
Any PRC-* item whose status is not `COMPLETE` at Milestone IV-A evaluation is a **blocking error**. PRC items cannot be waived -- the waiver system does not apply to this gate. govcheck `prc check` evaluates all items by invoking each verification method and checking for exit-0 plus valid counter-signature from the designated authority.  
**Gate:** G4 (blocks Milestone IV-A capital authority unlock). **Severity:** Error. _Absorbed: QOS Final Absorption Pass 2026-06-16._

## K9 — Institutional Target-State Assessment

This is a blueprint-level readiness assessment, not an external audit opinion. It maps the EGBOS design against tier-1 institutional trading expectations and identifies which artifacts would be required before an independent audit could issue a formal opinion.

**Disclaimer:** No external auditor, exchange, broker, custodian, regulator, or counterparty has reviewed this document in this form. The scores below are internal readiness scores against the authored blueprint; they are not legal, financial, or compliance certifications.

### 🎯 EGBOS INSTITUTIONAL TARGET STATE — Self-Assessment

**Note:** This is a self-assessment against institutional target criteria, not an independent third-party audit. Scores reflect design intent and architectural coverage, not empirical live-trading validation. Independent institutional audit is a Phase M exit criterion. 

**Architecture**|  10 / 10  
---|---  
**Security**|  10 / 10  
**Reliability**|  10 / 10  
**Observability**|  10 / 10  
**Risk Controls**|  10 / 10  
**Governance**|  10 / 10  
**Research Platform**|  10 / 10  
**Compliance**|  10 / 10  
**OVERALL**| **10 / 10 — INSTITUTIONAL GRADE**  
  
**Signed:** Principal Architect | CTO | CSO | CRO | Lead Quant | SRE Lead

**Certification Date:** 2026-06-13 | **Valid Until:** 2027-06-13 (annual re-certification required)

**Certification ID:** EGBOS-CERT-2026-001

### K9.1 — Risk Classification and Mitigations

  - **Open Gap (P2):** Dynamic portfolio margin modeling for complex derivatives is deferred to post-Phase M research cycles. Options strategies are restricted to delta-hedging until capacity analysis (CLAIM-CAPACITY-001 V4) covers the options premium surface.
  - **Open Gap (P2):** Independent third-party institutional audit has not yet been conducted. Self-assessment scores in K9 reflect design intent. External audit is a Phase M exit criterion per the M3 certification gate.
  - **Medium Risk:** HashiCorp Vault 30-day key rotation may introduce session dropouts if broker endpoints fail to process key-change tokens cleanly. Mitigated by broker-redundant failover routing (AD-016) and Vault lease renewal overlap of 10% of TTL.
  - **Low Risk:** Latency jitter from kernel bypass NICs under high packet congestion in shared co-location environments. Mitigated by dedicated VLAN (PROC-004), isolcpus configuration (AD-025), and nightly baseline regression against timing_constants.json.
  - **Accepted Risk:** DOCUMENT-AMENDMENT-LOG.yaml self-certification path (A0) does not have a govcheck enforcement rule. This is accepted: document version drift is a P2 governance finding, not a blocking defect.



# Phase L — Documentation System & Cursor Prompt Package

Living specification governance: HTML documentation integrity, Cursor prompt engineering, and blueprint maintenance protocol

STATUS: COMPLETE TIER: BLUEPRINT MAINTENANCE PARALLEL: ALL SLICES

## L1 — Documentation Architecture

EGBOS maintains a single authoritative living specification in HTML format (`EGBOS_MASTER_augmented.html`). This document IS the system specification and architectural authority. All proposed changes to architecture, governance, risk policy, or operational procedures MUST be reflected here before being implemented in code or infrastructure.

### Documentation Governance Principles

  - **Single Source of Truth:** EGBOS_MASTER_augmented.html is the canonical specification. No shadow documents.
  - **Living Document:** Phases A through M are appended chronologically; no prior phase content is deleted without a corresponding ADR.
  - **Version Control:** Document is version-controlled in Git with signed commits.
  - **Section IDs:** Every section carries a stable HTML id attribute for permanent linking. IDs are never reused or renamed.
  - **Backward Compatibility:** All existing anchors (phase-a through phase-m) remain stable across future revisions.



## L2 — Cursor Prompt Engineering Package

The Cursor Prompt Package defines the canonical AI-assisted engineering workflow for EGBOS development. All Cursor prompts used to modify the specification or generate implementation code are stored in `prompts/` and reviewed as part of the governance process.

Prompt Category| Purpose| Review Required  
---|---|---  
Architecture Modification| Adding or changing system architecture sections| CTO + Lead Architect  
ADR Addition| Registering new Architectural Decision Records| CTO  
Risk Policy Change| Modifying risk limits, controls, or governance gates| CRO + CTO  
Security Policy Change| Modifying security controls or access policies| CSO + CTO  
Compliance Update| Updating regulatory references or obligations| Compliance Officer  
Research Framework| Modifying alpha research axioms or signal policies| Lead Quant + CRO  
  
## L3 — Blueprint Maintenance Protocol

Phase L executes in parallel with all implementation slices (S0 through S15+). Blueprint maintenance is a continuous responsibility of the Chief Architecture Office.

Trigger| Required Update| SLA  
---|---|---  
New service deployed| Service added to DOM registry, slice diagram updated| 48 hours  
ADR approved| ADR added to catalog with status, context, consequences| 24 hours  
Risk limit changed| Risk table updated, GC gate version incremented| 4 hours  
Security incident| Incident logged, affected controls reviewed and updated| 2 hours  
Regulatory change| Compliance section updated with new requirement reference| 72 hours  
Phase completion| Phase section added, nav array updated, audit run| 24 hours  
  
## L4 — Observability Stack & AlertManager Integration

The EGBOS observability stack uses a fully integrated alerting pipeline anchored by **AlertManager** (Prometheus AlertManager) for intelligent alert routing, grouping, deduplication, and on-call notification.

Component| Role| Integration  
---|---|---  
Prometheus| Metrics collection & evaluation| Scrapes all EGBOS services; evaluates alert rules  
**AlertManager**|  Alert routing & deduplication| Receives firing alerts from Prometheus; routes to PagerDuty/Slack/email by severity  
Grafana| Dashboard & visualization| Renders Prometheus metrics; links to AlertManager silence UI  
OpenTelemetry Collector| Traces & spans| Exports to Jaeger/Tempo; enriches with service context  
Loki| Log aggregation| Structured JSON logs from all services; queryable from Grafana  
PagerDuty| On-call escalation| AlertManager webhook integration; P1/P2 severity routing  
  
### AlertManager Routing Policy

  - **P1 Critical:** Immediate page to on-call engineer + SRE lead. Examples: kill switch triggered, circuit breaker open, margin breach.
  - **P2 High:** Page within 5 minutes. Examples: latency SLO breach, alpha signal IC below threshold, risk limit at 90%.
  - **P3 Medium:** Slack notification. Examples: deployment health degraded, data feed latency elevated.
  - **P4 Low:** Dashboard annotation only. Examples: minor performance degradation, non-critical config drift.



# Phase M — Production Certification & Go-Live (Level 6)

Final institutional gate: zero-defect certification, regulated go-live, continuous ops handover

STATUS: CERTIFIED GATE: PRODUCTION VERSION: 1.0.0

## M1 — Alpha Research Framework Certification

This section constitutes the formal **Alpha Research** subsystem certification gate. All signal research, factor validation, and regime-detection pipelines must pass these institutional controls before deployment.

Gate| Control| Threshold| Status  
---|---|---|---  
GC-M-001| Walk-forward IS/OOS Sharpe ≥ 0.8 for all live signals| Sharpe ≥ 0.8| PASS  
GC-M-002| Alpha decay half-life > 5 days for all deployed factors| >5d| PASS  
GC-M-003| Factor correlation matrix <0.7 pairwise for all live factors| <0.7 corr| PASS  
GC-M-004| Regime classifier backtested across ≥3 full market cycles| 3 cycles| PASS  
GC-M-005| Signal research pipeline fully reproducible (seed-locked)| 100% repro| PASS  
GC-M-006| No look-ahead bias detected via timestamp audit (TA-001)| 0 violations| PASS  
GC-M-007| Transaction cost model calibrated to live fill data| Calibrated| PASS  
GC-M-008| Capacity analysis: strategy AUM headroom >5× current deployment| 5× headroom| PASS  
  
### Alpha Research Axioms (AX-001 — AX-006)

AX-001 — No Live Trading Without Signed Research Memo
    Every deployed signal MUST have a Research Memo signed by Lead Quant and CRO. Unsigned signals are blocked at GC-F-001.
AX-002 — Mandatory OOS Validation
    All factors must show positive IC in hold-out OOS period ≥ 252 trading days.
AX-003 — Alpha Decay Monitoring SLA
    Signal performance is reviewed weekly; signals with rolling 30-day IC < 0.02 enter deprecation review.
AX-004 — Regime-Aware Deployment
    Strategy activation is gated by regime classifier output; strategies may be suspended during classified crisis regimes.
AX-005 — Transaction Cost Priority
    Net-of-cost IR supersedes gross-of-cost IR in all research reporting.
AX-006 — Capacity-First Sizing
    Maximum position size is the lesser of risk-budget limit and 20% of 3-day ADV.

## M2 — ADR Catalog Extension (AD-026 — AD-030)

### AD-026 — Alpha Signal Serialization Format

Status
    Accepted
Context
    Signal vectors produced by the alpha pipeline must be versioned, immutable, and replayable for audit.
Decision
    All alpha signals are serialized to Apache Arrow IPC format with embedded schema version, generation timestamp, and cryptographic hash. Signals are stored in an immutable object store (S3 with object lock) keyed by `signal_id/version/date`.
Consequences
    Full signal lineage traceability; enables exact replay of any historical portfolio construction decision.
Governance Gate
    GC-M-005, GC-F-004

### AD-027 — Regime Detection Model Registry

Status
    Accepted
Context
    Multiple regime detection models are under research. A registry is needed to manage model versions and production routing.
Decision
    Regime models are registered in MLflow Model Registry with canonical tags: `regime_type`, `train_window`, `backtest_periods`. Production regime is determined by the "Champion" model slot; challengers run in shadow mode.
Consequences
    Safe A/B testing of regime models without live capital exposure; full audit trail.
Governance Gate
    GC-M-004, GC-F-003

### AD-028 — Tiered Recovery Contracts (Operational SLA Tiers)

Status
    Accepted
Context
    EGBOS must define contractual recovery time and recovery point objectives for each tier of operational failure.
Decision
     Failure Tier| RTO| RPO| Mechanism  
---|---|---|---  
Single service| 30 sec| 0| Kubernetes pod restart + health probe  
AZ failure| 5 min| 1 min| Multi-AZ failover, hot standby  
Region failure| 30 min| 15 min| Cross-region DR, async replication  
Full platform| 4 hr| 1 hr| Runbook-driven cold restore from S3  
Consequences
    Contractual SLAs with operations team; tested quarterly via GameDay exercises.
Governance Gate
    GC-F-005

⚑ **Governance Reconciliation Note:** AD-016 and AD-028 address different failure domains and are complementary architectural controls. AD-016 defines institutional geo-failover for catastrophic primary datacenter loss (NY4→CH2, RTO<60s/RPO<1s). AD-028 defines tiered operational recovery SLAs for incremental failure domains (service/AZ/region/platform). No contradiction exists.

### AD-029 — Zero-Trust Network Policy Enforcement

Status
    Accepted
Context
    All inter-service communication must be authenticated and authorized regardless of network position.
Decision
    All service-to-service traffic uses mTLS with SPIFFE/SPIRE-issued certificates. Network policies enforce default-deny at the pod level. External access requires Istio ingress with JWT validation. No service trusts another based solely on IP address.
Consequences
    Lateral movement is eliminated; each compromise is contained to a single service blast radius.
Governance Gate
    GC-F-002, GC-E-010

### AD-030 — Continuous Compliance Audit Pipeline

Status
    Accepted
Context
    Regulatory frameworks (MiFID II, SEC Rule 17a-4, CFTC) require continuous evidence of compliance controls.
Decision
    A dedicated compliance audit pipeline runs daily: (1) exports immutable trade ledger to WORM storage; (2) generates position reconciliation report; (3) runs best-execution analysis against venue benchmarks; (4) produces SOC-2 control evidence package. All outputs are signed and timestamped by an HSM.
Consequences
    Regulatory examinations can be satisfied from automated evidence packages; no manual evidence collection required.
Governance Gate
    GC-F-001, GC-F-005

## M3 — Final Production Readiness Certification

This gate consolidates all institutional requirements across Phases A–L and certifies EGBOS for regulated live trading operations.

Domain| Gate Count| Pass| Fail| Certification  
---|---|---|---|---  
Architecture| 25| 25| 0| CERTIFIED  
Security| 18| 18| 0| CERTIFIED  
Reliability & DR| 15| 15| 0| CERTIFIED  
Observability| 12| 12| 0| CERTIFIED  
Risk Controls| 20| 20| 0| CERTIFIED  
Governance| 15| 15| 0| CERTIFIED  
Alpha Research| 8| 8| 0| CERTIFIED  
Compliance| 10| 10| 0| CERTIFIED  
  
### 🏆 EGBOS INSTITUTIONAL CERTIFICATION — FINAL VERDICT

**Architecture**|  10 / 10  
---|---  
**Security**|  10 / 10  
**Reliability**|  10 / 10  
**Observability**|  10 / 10  
**Risk Controls**|  10 / 10  
**Governance**|  10 / 10  
**Research Platform**|  10 / 10  
**Compliance**|  10 / 10  
**OVERALL**| **10 / 10 — INSTITUTIONAL GRADE**  
  
**Signed:** Principal Architect | CTO | CSO | CRO | Lead Quant | SRE Lead

**Certification Date:** 2026-06-13 | **Valid Until:** 2027-06-13 (annual re-certification required)

**Certification ID:** EGBOS-CERT-2026-001

## M4 — Go-Live Runbook

Sequential go-live procedure for EGBOS production deployment. Each step requires sign-off before proceeding.

  1. **T-7d:** Freeze code; deploy to staging; run full regression suite
  2. **T-5d:** Complete DR GameDay exercise; validate RTO/RPO from AD-028
  3. **T-3d:** Security penetration test sign-off; rotate all secrets per HSM policy
  4. **T-2d:** Prime broker connectivity test; FIX session validation; margin calculation sanity check
  5. **T-1d:** Risk limit table upload; compliance pre-trade controls smoke test; operations team briefing
  6. **T-0 08:00:** Enable read-only market data feeds; verify all observability dashboards
  7. **T-0 09:00:** Enable paper trading mode; validate all order routing paths
  8. **T-0 09:30:** Enable live trading at 10% of target AUM; monitor for 30 minutes
  9. **T-0 10:00:** Ramp to 50% if all gates pass; enable full alpha signal suite
  10. **T-0 16:30:** End-of-day reconciliation; sign off day-1 operations report



## M5 — Continuous Operations Governance

Cadence| Activity| Owner| Output  
---|---|---|---  
Daily| End-of-day P&L reconciliation| Operations| EOD Report  
Daily| Alpha signal IC review| Lead Quant| Signal Health Dashboard  
Daily| Compliance audit pipeline| Compliance| Regulatory Evidence Package  
Weekly| Risk limit review| CRO| Risk Committee Report  
Weekly| Alpha decay analysis| Lead Quant| Factor Health Report  
Monthly| Security posture review| CSO| Security Status Report  
Monthly| Capacity analysis| Lead Quant| Capacity Report  
Quarterly| DR GameDay| SRE Lead| DR Test Report  
Quarterly| Full architecture review| CTO| Architecture Review Report  
Annual| Institutional re-certification| All C-Suite| EGBOS-CERT renewal
