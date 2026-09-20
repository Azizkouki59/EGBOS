# S5-REQ-004 — Canonical Snapshot Serialization Formal Ratification Package

**Document status:** APPROVED
**Prepared by:** Implementation Agent (S5 STEP 10)
**Spec reference:** S5-REQ-004 / `docs/S5_INSTITUTIONAL_SPEC.md` line 90–99
**Contract source:** `governance/amendments/S5_REQ004_SNAPSHOT_AMENDMENT.md`

---

## 1. Requirement

S5-REQ-004 requires 14 consecutive successful nightly deterministic replays
(byte-identical against the Tier-0 WAL log). This requires a canonical daily
snapshot of operational state that can initialize Night N+1 from Night N's
final state. The serialization mechanism must be cryptographically
deterministic, independent of process identity, and free of execution ephemera.

---

## 2. Serialization Contract

### 2A. Canonical Domain Ordering

Domains serialized in this exact sequence:

| Position | Domain | Source structure |
|---|---|---|
| 1 | **Positions** | `PositionEngine`: `position_`, `gross_exposure_`, `order_tag_`, `x_l_`, `next_sequence_` |
| 2 | **Risk Limits** | `RiskEnforcer`: `current_state_` (QuotingState enum), `last_heartbeat_tsc_` |
| 3 | **LOB State** | `LobEngine`: price-level arrays, `Top5BidsAsks` cache, anchor price, tick size |
| 4 | **Orders** | `OrderNode` records from `LobEngine::FixedPool`, sorted deterministically |

### 2B. Binary Encoding

| Property | Specification |
|---|---|
| Integer widths | Fixed: `int64_t`, `uint64_t`, `int32_t`, `uint32_t` (matching runtime types) |
| Signedness | Must exactly match the runtime domain field type |
| Byte order | Little-endian (host byte order on target platform) |
| Floating-point | None permitted (EGBOS uses fixed-point exclusively per AD-004) |
| Record length | Fixed length per domain; no variable-length encoding |
| String encoding | None (no string fields in canonical domains) |
| Padding | All alignment padding bytes MUST be explicitly zeroed before serialization |
| Alignment | No alignment requirements in the serialized blob; packed sequential write |

### 2C. Snapshot Header

The serialized blob begins with a fixed 24-byte header:

| Offset | Width | Value | Description |
|---|---|---|---|
| 0 | 8 bytes | `0x4547424F5353484D` | Magic ("EGBOSSHM" in little-endian ASCII) |
| 8 | 4 bytes | `1` | Format version |
| 12 | 4 bytes | `1` | Schema version |
| 16 | 8 bytes | SHA-256 (first 8 bytes) | Partial hash for quick rejection check |

Full SHA-256 is appended as a 32-byte trailer after all domain payloads.

**Compatibility rules:**
- Magic mismatch → `TERMINAL_HALT` (do not proceed with hydration)
- Format version mismatch → `TERMINAL_HALT`
- Schema version mismatch → `TERMINAL_HALT`
- No partial-compatibility or version negotiation is permitted

### 2D. Deterministic Ordering for OrderNode Records

`OrderNode` records contain linked-list pointers (`next`, `prev`) that are
process-local and non-deterministic. The canonical ordering rule:

1. Primary sort key: `price_index` — ascending
2. Secondary sort key: `order_id` — ascending

No implementation may depend on:
- Hash-map iteration order
- Pointer addresses
- PID, TID, or thread-local storage values
- Runtime allocation order from `FixedPool`

### 2E. Excluded Runtime State

The following MUST NOT be serialized:

| Category | Excluded items |
|---|---|
| Process identity | `pthread_t`, `PID`, `TID` |
| SPSC internals | `head_`, `tail_`, `head_cache_`, `tail_cache_`, buffer contents |
| Pointer fields | `OrderNode::next`, `OrderNode::prev`, any SHM-mapped pointer |
| Cache padding | Any `alignas()` padding array not carrying business data |
| Runtime sync | `std::atomic<>` wait state, `std::mutex` internal state, lock ownership |
| Execution metadata | `producer_id_`, `consumer_id_` thread identity fields |

---

## 3. Hydration Contract

```
SNAPSHOT_N → Night N+1 SHM initialization
```

**Required sequence:**

1. **Magic verification** — read 8-byte magic; abort if not `0x4547424F5353484D`
2. **Version verification** — read format and schema versions; abort on mismatch
3. **Hash verification** — recompute `SHA256(all domain payloads)` and compare
   to stored trailer; abort if mismatch
4. **Domain deserialization** — apply in canonical order:
   - Write `PositionEngine` fields
   - Write `RiskEnforcer` fields
   - Write `LobEngine` price-level arrays
   - Write `OrderNode` pool entries from sorted canonical records
5. **Deterministic pointer reconstruction** — rebuild `OrderNode::next` and
   `OrderNode::prev` chain from the canonically sorted list without using any
   pointer from the snapshot blob
6. **Post-hydration invariant check** — verify that the recomputed state hash
   equals the stored `initial_state_hash(N+1)` before allowing any engine
   subsystem to read from SHM

**Failure behavior at any step:** `TERMINAL_HALT` — engine must not start with
a corrupt or mismatched snapshot.

---

## 4. Continuity Invariant

```
initial_state_hash(N+1) == final_state_hash(N)
```

where:

```
state_hash(X) = SHA256(canonical_serialized_blob(X))
```

Replay may not proceed across a night boundary unless this invariant holds.

---

## 5. What Is Not Yet Defined

This ratification package depends on precise field-by-field struct layout for:

- `PositionEngine` — inferred from `engine/core/position_engine.h`
- `RiskEnforcer` — inferred from `engine/core/risk_enforcer.h`
- `LobEngine` — inferred from `engine/l1/lob_engine.h`

Exact byte offsets, field counts, and pool capacity constants must be verified
against the compiled structs before the serializer implementation begins.

---

## 6. Implementation Authorization

**NONE.** Serializer and deserializer implementation remains strictly blocked
until this ratification document receives explicit governance approval.

Do NOT modify `shm_layout.h`. Do NOT modify runtime structures.
Do NOT promote claims. Do NOT modify existing evidence classifications.

---

## 7. Governance Decision

```
DECISION: APPROVED
```

```
[x] APPROVED
[ ] REJECTED
[ ] AMENDMENT REQUIRED

Authority: Governance Board

Name:      Governance Board

Date:      2026-08-18T16:30:38Z

Decision Reference: EGBOS-GOV-DECISION-2663389-S5-12
```
