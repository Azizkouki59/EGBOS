# RFC-L2-REPORT-TRACEABILITY
**Status:** Informational (not a feature contract)  
**Scope:** Traceability from `engine/l2/rapport.txt` research report to production code  
**Date:** 2026-10-11

---

## 1. Purpose

This RFC documents which mathematical constructs in the research report
(`engine/l2/rapport.txt`) have been:

- **Ratified** and wired into production, with exact governance citation
- **Blocked** and why (missing inputs, no owner decision, no parity reference)
- **Out of scope** for the current production slice

It does NOT constitute a feature contract. No production parameter may be derived
solely from this document. All ratified parameters must appear in
`governance/claims/PARAMETER_REGISTRY` and a feature-specific governance claim.

---

## 2. Feature-by-Feature Disposition

### 2.1 Hawkes Intensity (univariate, §"Trade Intensity and Point Processes")

| Attribute | Value |
|---|---|
| Report section | "Trade Intensity and Point Processes → Hawkes Point Processes" |
| Production status | **RATIFIED** |
| Feature ID | `0x0001` |
| Governance claim | `governance/claims/FEAT-0001-Hawkes-Intensity.md` |
| Implementation | `engine/l2/feature_compute.cpp` |
| Parameters | μ=0.1, α=0.05, β=0.01 — ALL come from `FEAT-0001-Hawkes-Intensity.md`, NOT from the report |
| Report formula used? | Structural form λ(t)=μ+Σα·exp(−β·(t−tᵢ)) adopted; parameters not adopted from report |
| Report says | Bivariate (B/S) with 6 free parameters; we implement univariate with ratified constants |
| Divergence from report | Univariate (pooled) vs. bivariate; snapshot-based (not trade-print) input |

> [!NOTE]  
> The report's bivariate Hawkes formulation (§"Asymmetric Intensity Differential")
> and its parameter discussion are **research input only**. The production FEAT-0001
> is explicitly univariate and snapshot-based per `FEAT-0001-Hawkes-Intensity.md`.
> Do NOT replace FEAT-0001 with the bivariate trade-print variant without a new ratified claim.

### 2.2 VPIN (§"Volume-Synchronized Probability of Informed Trading")

| Attribute | Value |
|---|---|
| Report section | "Order Flow and Imbalance Dynamics → VPIN" |
| Production status | **BLOCKED** |
| Blocking RFC | `RFC-004-L0-TRADE-FEED-VPIN-INPUT.yaml` |
| Root cause | No trade stream (`@aggTrade`) subscribed at L0; `EventType::TRADE` is never emitted |
| Action required | Separate L0 RFC for aggTrade ingestion before any VPIN math |

### 2.3 Order Flow Imbalance — OFI / Multi-Level OFI (§"Order Flow and Imbalance Dynamics")

| Attribute | Value |
|---|---|
| Report section | "Order Flow Imbalance (OFI)", "Multi-Level OFI" |
| Production status | **Level-1 OFI RATIFIED (FEAT-0002)**; Multi-Level OFI (Level > 1) BLOCKED |
| Contract | `governance/claims/FEAT-0002-Order-Flow-Imbalance.md` |
| Feature ID / Offset | `0x0002` / payload bytes 4–7 (`float`) |
| Formula | Level-1 CKS: `e_n^b - e_n^a` using reconstructed absolute prices; cold start / gap = 0.0f |
| Multi-Level OFI status | BLOCKED — parameter λ for depth decay weighting not ratified |

### 2.4 Queue Imbalance — QI (§"Static Queue Imbalance")

| Attribute | Value |
|---|---|
| Report section | "Static Queue Imbalance" |
| Production status | **BLOCKED — No ratified contract** |
| Formula | `QI = (Q_bid − Q_ask) / (Q_bid + Q_ask)` — formula is parameter-free |
| Missing | Feature ID, payload offset, validity bit assignment |
| Note | Formula requires no calibration but no claim assigns its slot position |

### 2.5 Stoikov Micro-Price (§"The Micro-Price")

| Attribute | Value |
|---|---|
| Report section | "Price Microstructure Indicators → The Micro-Price" |
| Production status | **BLOCKED — Missing calibration parameter** |
| Missing | `ω(δ_t)` — the spread-scaling function; requires empirical calibration or explicit owner decision |
| Owner decision | Not provided → implementation is blocked |

### 2.6 Book Dispersion / HHI (§"Liquidity and Book Dispersion")

| Attribute | Value |
|---|---|
| Report section | "Book Dispersion", "Herfindahl-Hirschman Index (HHI)" |
| Production status | **BLOCKED — No ratified contract** |
| Missing | Feature ID, payload offset, `w_k` weight scheme (owner decision required) |

### 2.7 Bid-Ask Spread Dynamics (§"Bid-Ask Spread Dynamics")

| Attribute | Value |
|---|---|
| Report section | "Bid-Ask Spread Dynamics" |
| Production status | **BLOCKED — No ratified contract** |
| Missing | Feature ID, payload offset, validity bit |

### 2.8 Normalisation / Online Z-Scoring (§"Feature Normalization")

| Attribute | Value |
|---|---|
| Report section | "Feature Normalization, Orthogonalization, and Combination" |
| Production status | **BLOCKED — Missing parameters** |
| Missing | Decay factor τ, clip bound c_max — require owner decisions |

---

## 3. Parameters Sourced from the Report: NONE

The research report (`rapport.txt`) provides **no parameter values** that have
been adopted directly into production. All ratified production parameters
are enumerated in `governance/claims/PARAMETER_REGISTRY`.

---

## 4. What Changes This RFC Does NOT Authorize

- This RFC does not ratify any feature beyond FEAT-0001.
- This RFC does not assign any feature ID, payload offset, or validity bit.
- This RFC does not change `engine/l2/candidates/` isolation rules.

---

## 5. References

| Document | Role |
|---|---|
| `engine/l2/rapport.txt` | Research input — NOT authoritative |
| `governance/claims/FEAT-0001-Hawkes-Intensity.md` | Ratified feature contract |
| `governance/claims/PARAMETER_REGISTRY` | All ratified production parameters |
| `governance/rfcs/RFC-003-L2-HAWKES-VPIN-FEATURE-SCOPING.yaml` | Scope decision: Hawkes ratified, VPIN blocked |
| `governance/rfcs/RFC-004-L0-TRADE-FEED-VPIN-INPUT.yaml` | VPIN prerequisite RFC |
