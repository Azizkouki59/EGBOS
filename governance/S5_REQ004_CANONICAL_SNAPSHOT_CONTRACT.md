# S5-REQ-004 — Canonical Snapshot Contract

**Governance Status:** CONTRACT DEFINITION ONLY (Implementation Blocked)
**Target Release:** V4 / S5

## 1. Background

S5-REQ-004 requires a daily canonical snapshot serialization mechanism for cross-night 14-day deterministic replay (LOB state handoff).

## 2. Blocked Implementation: Raw /dev/shm Hashing

The previous approach of hashing the entire raw `/dev/shm` footprint is **REJECTED** and implementation is HALTED.
Raw SHM contains non-deterministic execution ephemera (PIDs, TIDs, pthread constructs, uninitialized padding, caching layer boundaries) that violate the cryptographic determinism requirements of the canonical state hash.

## 3. The Canonical State Contract

To support cross-night replay, the serialization contract must strictly separate canonical deterministic business state from execution environment ephemera.

### 3.1 Included State (The Canonical Footprint)
Only the following deterministic domains shall be serialized into `SNAPSHOT_N`:
- **Positions:** `PositionEngine` net exposure state per instrument.
- **Risk Limits:** `RiskEnforcer` limit utilizations and static constraints.
- **Orders:** `OrderRouter` live resting order state.
- **Order Book (L1):** `LobEngine` price level state.

### 3.2 Excluded State (Execution Ephemera)
The following MUST NOT be serialized:
- `pthread_t`, `PID`, `TID` identifiers.
- Hardware cache-alignment padding arrays (unless guaranteed zeroed).
- `SpscRing` internal pointers, sequence numbers, and buffer contents.
- L4/L5 execution runtime boundaries and ownership primitives (locks, atomic wait queues).
- Producer/Consumer identity tokens.

## 4. Deterministic Hash Guarantee

The snapshot infrastructure must guarantee the following invariant across process restarts:
```
initial_state_hash(N+1) == final_state_hash(N)
```
Where `state_hash` is defined as `SHA256(canonical_snapshot_binary_blob)`.

## 5. Required Action

Governance must formally approve this contract definition.
Implementation of the serializer and deserializer logic is structurally blocked until this contract is approved.
