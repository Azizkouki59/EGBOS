# AD-031 — Decoupling Paper Trading from Production Hardware Qualification

## Status
Active

## Context
The Phase I specification required all execution plane slices (S0–S7) and specifically S6 (Production Hardware Qualification) and S7 (Capital Deployment Infrastructure) to be passed before Paper Trading (MS-005) could be authorized. This requirement artificially coupled the validation of the software execution plane with the procurement and installation of physical production hardware (PROC-001 through PROC-004).

Paper trading is a fundamental zero-capital software validation mechanism. Blocking paper trading behind Tier-3 colocation, 32-core EPYC servers, and 10GbE network interfaces reduces engineering velocity and delays end-to-end integration testing without providing any capital safety benefit. The software execution engine can and must be validated on engineering hardware using simulated fills and zero live capital.

## Decision
We are separating **Engineering Execution Qualification** from **Production Infrastructure Qualification**.

1.  **S6 is redefined as Engineering Execution Qualification.** It requires the software workstream to be complete, including L2 (book state), L3 (risk controls), L4 (execution safety), the paper execution path, and safety monitors. S6 requires a 48-hour staging run on available engineering hardware, not production hardware.
2.  **MS-005 (Paper Trading Authorization) is unlocked at S6-exit.** Paper trading enforces zero real capital, zero live orders, and termination of the order path at simulated fills. It does not require S7.
3.  **S7 is redefined as Capital Deployment Infrastructure & Production Qualification.** It absorbs the production hardware requirements (PROC-001 through PROC-004) previously in S6. S7 must be passed before any live capital can be deployed.
4.  **MS-006 (IV-A Live Capital Gate)** remains dependent on S7 and the MS-005 clean-operation duration.

## Consequences
- **Positive:** Unblocks end-to-end paper trading and execution engine development on engineering hardware.
- **Positive:** Preserves immutable production hardware requirements for real capital deployment.
- **Negative/Risk:** Developers must explicitly ensure that paper trading mode never routes to a live exchange, as the system is now capable of full execution logic on non-production networks. This is mitigated by the paper execution path boundary.

## Implementation
Enforced by `RFC-001` which amends `docs/EGBOS_MASTER.md`, `governance/exit-gates/SLICE-S6-exit-v1.yaml`, and creates `governance/exit-gates/SLICE-S7-exit-v1.yaml`.
