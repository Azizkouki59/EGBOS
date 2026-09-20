# GOVERNANCE BOARD DECISION — S5 L0 ARCHITECTURAL DECISIONS

Decision Authority: Governance Board
Decision Reference: EGBOS-GOV-DECISION-2026-009
Decision Timestamp: 2026-08-23T09:04:00Z
Approval Status: APPROVED

---

## Decision ID: GOV-S5-L0-ARCH-DECISIONS

### D1 — Multicast Failure Policy

```
B = EXPLICIT_NON_OPERATIONAL_STATE
```

A failed multicast membership/join MUST NOT be treated as an operational production feed.
The listener must enter an explicit non-operational/fault state.

### D2 — Listener Termination Synchronization

```
A = std::atomic<bool> with memory_order_relaxed
```

Concurrent `request_stop()` / `run_loop()` stop flag must use `std::atomic<bool>`.
`volatile` is not a substitute for atomic synchronization.

### D3 — Socket Error Contract

```
PROPOSED_FAULT_MATRIX — APPROVED
```

| Error | Classification | Action |
|---|---|---|
| EAGAIN | Non-terminal | Continue / staleness check |
| EWOULDBLOCK | Non-terminal | Continue / staleness check |
| EINTR | Non-terminal | Retry |
| EBADF | Terminal | Transition to fault state |
| ECONNRESET | Terminal | Transition to fault state |
| Other unrecoverable | Terminal | Transition to fault state |

### D4 — Timestamp Contract

```
AUTHORITATIVE_REQUIREMENT — ENFORCE CLOCK_MONOTONIC_RAW
```

- Clock: `CLOCK_MONOTONIC_RAW`
- Unit: nanoseconds
- Domain: strict monotonic
- `std::chrono::high_resolution_clock`: REMOVED from L0 timing path
- `CLOCK_REALTIME`: FORBIDDEN (GC-E-011)

### D5 — Platform Boundary

```
AUTHORITATIVE_REQUIREMENT — LINUX TIER-0
```

- Production Tier-0: Linux
- Windows: DEVELOPMENT / TEST ONLY

### D6 — Polling Model

```
IO_URING
```

- Latency budget: NOT_YET_SPECIFIED
- CPU budget: NOT_YET_SPECIFIED
- Architecture: follows existing S1 io_uring pattern

### D7 — Feed Source Architecture

```
REQUIRE_PROVENANCE_ABSTRACTION
```

Strict separation between DevelopmentTestFeed and ProductionMulticastFeed.
TEST DATA != LIVE_OPERATIONAL EVIDENCE.

### D8 — UDP Buffer Size

```
SOURCE_DEPENDENT — WAIT FOR REAL FEED SPECIFICATION
```

Current 2048-byte placeholder remains unchanged.

### D9 — Live Source Provenance

```
REQUIRE_PROVENANCE_BINDING
```

LIVE_OPERATIONAL evidence must be explicitly bound to a legitimate production
market-data source. LOCAL/SYNTHETIC/REPLAY DATA != LIVE_OPERATIONAL EVIDENCE.

---

## Operational State (Unchanged)

```
REAL_MARKET_DATA_SOURCE = NOT_PROVISIONED
LIVE_OPERATIONAL = NONE
REQ-003 = OPEN / BLOCKED
REQ-004 = OPEN / BLOCKED
V4 = DEFERRED
S5 = BLOCKED_NO_LIVE_OPERATIONAL_SOURCE
```

This decision authorizes engineering work only. It does NOT constitute
production operation, LIVE_OPERATIONAL evidence, or V4 qualification.
