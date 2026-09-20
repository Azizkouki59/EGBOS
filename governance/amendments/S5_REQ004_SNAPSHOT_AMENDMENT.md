# S5-REQ-004 — Canonical Snapshot Serialization Amendment

## A. Canonical Domain Ordering
The canonical state blob MUST be serialized sequentially in the following domain order based on SHM architecture:
1. Positions (`PositionEngine` state: gross exposure, position, order tag, x_l)
2. Risk Limits (`RiskEnforcer` state: quoting state, heartbeat)
3. LOB state (`LobEngine` top-level arrays/Top5)
4. Orders (Mapped deterministically from `LobEngine::OrderNode` list)

## B. Binary Encoding
- **Integer widths:** Strictly fixed-width (`int64_t`, `uint64_t`, `int32_t`, `uint32_t`).
- **Signedness:** Must match the runtime domain types identically.
- **Byte order:** Little-endian (host byte order).
- **Format:** Fixed length binary records for each domain.
- **Padding rules:** Any alignment padding MUST be explicitly zeroed before serialization.

## C. Versioning
- **Snapshot Magic:** `0x4547424F5353484D` ("EGBOSSHM")
- **Format Version:** `1`
- **Schema Version:** `1`
- **Compatibility Rules:** Strict exact matching only.
- **Rejection Behavior:** Abort hydration (TERMINAL_HALT) if magic or versions do not exactly match.

## D. Deterministic Ordering
For unordered structures:
- `LobEngine::OrderNode` elements MUST be sorted ascending by `price_index`, then ascending by `order_id` (deterministic tie-breaking).
- No implementation may depend on hash-map iteration order, pointer addresses, PID/TID values, or runtime allocation order.

## E. Runtime-State Exclusion
The following execution ephemera MUST be strictly excluded from the canonical serialization:
- `pthread_t`, `PID`, `TID` identifiers.
- Hardware cache padding arrays.
- `SpscRing` internal pointers, sequence numbers, and buffer contents.
- L4/L5 locks, atomic wait queues, and runtime synchronization metadata.
- Process-local pointers (e.g., `OrderNode* next`, `prev`).

## F. Hydration
`SNAPSHOT_N → SHM state for NIGHT N+1`
- **Initialization Order:** Magic/version verification -> Hash verification -> Domain deserialization (Positions -> Risk Limits -> LOB -> Orders).
- **Validation:** Abort on any deserialization bounds violation or truncation.
- **Post-hydration:** Internal pointers (`OrderNode* next`, `prev`) must be deterministically reconstructed from the canonical data.

## G. Hash Contract
`SHA256(canonical_serialized_state)` MUST be computed exclusively over the deterministic binary layout.
Invariant enforced before replay: `final_state_hash(N) == initial_state_hash(N+1)`.
