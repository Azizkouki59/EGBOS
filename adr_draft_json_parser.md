# Architecture Decision Record (ADR)
## Zero-Allocation Binance depthUpdate JSON Parser & L1 Consumer

## Status
**DRAFT / PROPOSED** — not yet approved. No implementation should be treated
as final until this status changes explicitly, by a real decision, not by this
document being written. Resolving the three open questions below (see
"Resolved Decisions") does not constitute ADR approval.

---

## Context
- **Data Flow Dead-End**: L0 (`FeedIngestor`) currently forwards raw, unparsed
  Binance JSON bytes into the `MarketEventsRegion` SPSC ring with no consumer
  draining it.
- **S2-REQ-002 (Gap Detection)**: Gap detection is currently deferred/stubbed
  pending real `U`/`u` sequence-number extraction from JSON (see `ingestor.cpp`
  test comments, tests marked `_S2REQ002NotYetImplemented`).
- **Governance**: GC-E-010 (zero dynamic allocation on hot path) strictly
  applies to this component. General-purpose JSON libraries that heap-allocate
  (e.g., `nlohmann::json`, heap-mode `simdjson`) are not permissible.
- **Target Payload**: Verbatim schema fetched from
  `binance/binance-spot-api-docs` `master` branch (confirmed current; see
  Evidence section). The `depthUpdate` payload is:

  ```javascript
  {
      "e": "depthUpdate",     // Event type
      "E": 1672515782136,     // Event time (milliseconds)
      "s": "BNBBTC",          // Symbol
      "U": 157,               // First update ID in event
      "u": 160,               // Final update ID in event
      "b": [                  // Bids to be updated
          [
              "0.0024",       // Price level to be updated  ← quoted string, not number
              "10"            // Quantity                   ← quoted string, not number
          ]
      ],
      "a": [                  // Asks to be updated
          [
              "0.0026",       // Price level to be updated
              "100"           // Quantity
          ]
      ]
  }
  ```

  **Key schema facts**:
  - `"b"` and `"a"` entries are **quoted decimal strings**, not bare JSON numbers.
  - `"U"` and `"u"` are **bare JSON integers** (no quotes). No explicit type
    bound is stated in the docs; see Resolved Decisions §3 for the justified
    type choice.
  - `"E"` and `"s"` are not consumed by the parser and are skipped.

---

## Decision

### 1. Hand-Rolled, Zero-Allocation Parser
A purpose-built parser extracting *only* the required fields from a raw JSON
byte buffer:
- `"e"` (event type string — must equal `"depthUpdate"` exactly)
- `"U"` (first update ID, parsed as `uint64_t`)
- `"u"` (final update ID, parsed as `uint64_t`)
- `"b"` and `"a"` arrays — each entry: price as fixed-point `int64_t` using
  `kPriceScale = 10,000,000` (10^7), qty as fixed-point `int64_t` using the
  same scale. See Resolved Decisions §2.

All other fields (`"E"`, `"s"`, any future additions) are skipped via
zero-allocation token fast-forwarding. The parser makes no assumptions about
field ordering within the JSON object.

### 2. Caller-Provided Fixed-Size Buffers
All output is written into a caller-provided `ParsedDepthUpdate` struct with
`std::array<PriceQty, kMaxUpdatesPerPacket>` for bids and asks respectively
(`kMaxUpdatesPerPacket = 256`, see Resolved Decisions §1). No `std::string`,
`std::vector`, or heap allocation of any kind.

### 3. Fail-Closed Malformed Input Handling
The parser enforces strict FAIL-CLOSED behavior: any deviation from the
expected format causes immediate rejection, an error return, and no partial
population of the output struct. The full output struct must be treated as
invalid on any error return. Error conditions:

| Condition | Behavior |
|-----------|----------|
| Truncated JSON (buffer ends mid-object/array/string) | REJECT — return error |
| Missing required field (`"u"`, `"b"`, `"a"`, `"e"`) | REJECT — return error |
| Unexpected event type (`"e"` ≠ `"depthUpdate"`) | REJECT — return error |
| Price/qty string non-numeric or empty | REJECT — return error |
| Price/qty in scientific notation (e.g., `"1e-5"`) | REJECT — return error |
| Price/qty has excess fractional precision | REJECT — return error (PRECISION_EXCEEDED) |
| Price/qty overflows `int64_t` after scaling | REJECT — return error |
| Array entry arity ≠ exactly 2 (`[price, qty]`) | REJECT — return error |
| `"b"` or `"a"` array length > `kMaxUpdatesPerPacket` | REJECT — return error |
| `U` > `u` (malformed sequence range) | REJECT — return error |

**Error Signaling**: Parse failures integrate with the existing `FeedState`
mechanism in `FeedIngestor`. A malformed packet triggers a transition to a
degraded state (exact `FeedState` enum value TBD at implementation time — must
not invent a new error channel outside the existing state machine).

### 4. L1 Consumer Loop Architecture (Design Sketch — Not Yet Implemented)
The L1 consumer runs on a dedicated thread, continuously polling
`MarketEventsRegion`.

```
loop:
  slot ← MarketEventsRegion.pop()
  if empty: continue (no sleep — L1 core is dedicated)

  result ← parse_depth_update(slot.data, sizeof(slot.data), &parsed)
  if result != OK:
      FeedState ← DEGRADED
      GovernanceFlags.feed_stale ← true
      continue  // LobEngine state is NOT touched

  // Sequence gap detection (S2-REQ-002)
  // TEMPORARY: snapshot recovery is not yet designed (see Consequences).
  // Any detected gap — whether from ring overflow or parsed.U mismatch —
  // results in HALT/DEGRADE, not attempted recovery. This is conservative
  // and consistent: a LOB that has missed updates is in an unknown state
  // and must not feed downstream components. Gap recovery will be a
  // separate, explicit design task before L1 is production-capable.
  if parsed.U != expected_next_update_id:
      FeedState ← DEGRADED          // same path as parse failure
      GovernanceFlags.feed_stale ← true
      continue  // do NOT apply stale/gapped updates to LobEngine

  // Apply updates to LobEngine
  for each entry in parsed.bids:
      if qty == 0: LobEngine.cancel_order(price)
      else:        LobEngine.modify_order(price, qty)

  for each entry in parsed.asks:
      if qty == 0: LobEngine.cancel_order(price)
      else:        LobEngine.modify_order(price, qty)

  LobSnapshotsRegion ← LobEngine.generate_top5_snapshot()
  expected_next_update_id = parsed.u + 1
```

**LOB atomicity on parse failure**: The parser validates the entire packet
*before* any `LobEngine` mutations. A parse failure mid-stream leaves the
`LobEngine` untouched and in a consistent prior state. This is enforced by
the two-phase design (parse fully → mutate only on `OK`).

**SPSC ring overflow and sequence gaps — current behavior**: Both SPSC ring
overflow and a `parsed.U` mismatch result in `FeedState → DEGRADED` and
`GovernanceFlags.feed_stale ← true`. The LOB is not mutated. This is
conservative: a LOB that has missed updates is in an unknown state and
must not feed downstream components. There is no active recovery path.
Snapshot-based re-synchronization is the correct long-term solution but is
**not yet designed**; it must be resolved before L1 is production-capable
(see Consequences). The pseudocode above reflects this actual current
behavior, not an aspirational one.

---

## Consequences
- **New files required**: `engine/l1/depth_parser.h`, `engine/l1/depth_parser.cpp`
  (the parser as a standalone unit), and a new L1 consumer binary or thread
  entry point.
- **Integration with `FeedIngestor`**: The `FeedState` enum needs at minimum
  one new state representing parse-level failures (distinct from the existing
  staleness/gap states). Exact naming deferred to implementation.
- **Test Coverage Required** (before this component is considered real):
  - Valid `depthUpdate` round-trip (parse → LobEngine mutations produce correct state)
  - Each malformed condition in the table above, individually
  - Exactly `kMaxUpdatesPerPacket` entries (boundary — must succeed)
  - `kMaxUpdatesPerPacket + 1` entries (boundary + 1 — must fail-closed)
  - Zero-entry `"b"` and `"a"` arrays (valid, common — must succeed)
  - Mixed zero-qty (cancel) and non-zero-qty (update) entries in one packet
  - Buffer exactly at `sizeof(MarketEventSlot::data)` = 128 bytes (truncation
    risk — real JSON will almost always exceed this; see Finding 2 / Finding 9
    from prior audit re: silent truncation in `normalize_and_forward`)
- **S2-REQ-002 Gap Detection**: Tests currently marked
  `_S2REQ002NotYetImplemented` must be reverted to real gap-detection
  assertions once `U`/`u` extraction is live. **This ADR does not un-defer
  those tests.** That is a separate, explicit follow-up task requiring its own
  decision point.
- **Snapshot Recovery**: SPSC ring overflow and large sequence gaps both
  require a Binance snapshot REST call + LOB re-hydration path. This is
  **not designed** and must be resolved before L1 is production-capable.

---

## Resolved Decisions
*(These were open questions; they are now resolved with empirical justification.
 Resolving them does not constitute ADR approval — Status remains DRAFT/PROPOSED.)*

### R1 — kMaxUpdatesPerPacket = 256

**Decision**: `kMaxUpdatesPerPacket = 256`. On exceeding this bound: reject
the entire packet (fail-closed), transition `FeedState` to degraded. Do not
truncate or partially process.

**Justification**: A 3-minute empirical capture on `btcusdt@depth` (testnet)
recorded a maximum of 42 bid entries and 42 ask entries per message across 119
messages. However, the p99 equalled the observed max (42), indicating the
short sample window likely did not capture the true tail of the distribution.
Production conditions — flash crashes, major news events, illiquid periods —
will produce spikes not seen in a quiet 3-minute testnet window. 256 is set
well above the observed max to give real headroom for such events, while
remaining small enough that `sizeof(ParsedDepthUpdate)` stays
stack-comfortable (256 entries × 16 bytes = 4 KB per side, 8 KB total for
bid + ask arrays combined).

The 16-bytes-per-entry figure assumes `PriceQty = {int64_t price; int64_t qty}`
(8 + 8 = 16 bytes, no padding required since both fields are naturally
aligned). **`PriceQty` does not yet exist in the codebase** — it will be
defined in `engine/l1/depth_parser.h`. If the implementation adds fields or
the compiler inserts unexpected padding, this arithmetic must be re-verified
against `sizeof(PriceQty)` at that time. A `static_assert(sizeof(PriceQty)
== 16)` should be added to `depth_parser.h` to make any deviation a build
error rather than a silent size change.

**Evidence**: See Evidence section — empirical capture statistics.

---

### R2 — kPriceScale = 10,000,000 (10^7), BTCUSDT Only

**Decision**: Raise `kPriceScale` from 100,000 (10^5) to 10,000,000 (10^7)
for this parser. **Scope: BTCUSDT only.** Any other symbol requires separate
verification against its own `PRICE_FILTER.tickSize` and `LOT_SIZE.stepSize`
before this parser is reused.

**Justification**: The BTCUSDT `stepSize` (quantity precision) is `0.00001000`
= 5 decimal places. The existing `kPriceScale = 10^5` covers this with exactly
zero headroom — a single extra decimal place in a Binance quantity string would
silently truncate. Raising to `10^7` provides 2 decimal places of headroom on
quantity and 5 decimal places of headroom on price (BTCUSDT `tickSize = 0.01`
= 2 decimal places). The cost is zero at parse time: multiplying a parsed
decimal by 10^7 instead of 10^5 is identical work. The benefit is eliminating
a silent truncation risk that only becomes visible on edge-case quantity values.

**BTCUSDT-specific scope**: This scale is empirically verified for BTCUSDT
only. The `baseAssetPrecision` and `quotePrecision` are both 8 on testnet, and
the worst-case field (`stepSize = 0.00001`) has 5 significant decimal digits —
within 10^7 with margin. A symbol with `stepSize < 0.0000001` would overflow
this scale, so the symbol-scoping constraint is load-bearing, not cosmetic.

**Evidence**: See Evidence section — `exchangeInfo` query results.

---

### R3 — U/u Update IDs parsed as uint64_t

**Decision**: Parse `"U"` and `"u"` as `uint64_t`.

**Justification**: The Binance docs display `"U"` and `"u"` as bare JSON
integers with labels "First update ID in event" and "Final update ID in event"
but **do not state an explicit type bound**. The justified reasoning for
`uint64_t`:

1. **Scale**: Binance update IDs are monotonically increasing per symbol since
   exchange inception. BTCUSDT update IDs are already in the high billions
   (observable in live stream output), exceeding the range of `int32_t`
   (2.1 billion) and approaching the range of `uint32_t` (4.3 billion).
   `int64_t` would be safe for operational lifetime, but `uint64_t` is more
   semantically correct (IDs are never negative) and provides the full
   unsigned 64-bit range (~1.8 × 10^19).
2. **Ordering invariant**: The parser will enforce `U ≤ u` as a validity check
   (see Decision §3 table). This invariant is only meaningful if both fields
   are the same type — `uint64_t` for both is consistent.
3. **JSON parsing**: Bare JSON integer parsing to `uint64_t` is
   straightforward. The parser will reject any value that is negative, has a
   decimal point, or exceeds `UINT64_MAX`.

---

## Evidence

### E1 — Verbatim depthUpdate Schema (master branch, confirmed current)

Fetched directly from
`https://raw.githubusercontent.com/binance/binance-spot-api-docs/master/web-socket-streams.md`
on 2026-09-13. The `master` branch was confirmed as the current live branch
(941 commits, all repo file links point to `master`; no versioned or `main`
branch exists). The `testnet/web-socket-streams.md` subdirectory contains an
**identical** `depthUpdate` section — no schema divergence between production
and testnet docs.

```javascript
// Source: binance/binance-spot-api-docs master, byte 19999 of 22958
{
    "e": "depthUpdate",     // Event type
    "E": 1672515782136,     // Event time
    "s": "BNBBTC",          // Symbol
    "U": 157,               // First update ID in event
    "u": 160,               // Final update ID in event
    "b": [                  // Bids to be updated
        [
            "0.0024",       // Price level to be updated
            "10"            // Quantity
        ]
    ],
    "a": [                  // Asks to be updated
        [
            "0.0026",       // Price level to be updated
            "100"           // Quantity
        ]
    ]
}
```

### E2 — Empirical Array Size Capture

**Method**: Python asyncio WebSocket client connected to
`wss://stream.testnet.binance.vision/ws/btcusdt@depth`. 180 seconds runtime,
2026-09-13.

**Results**: 119 `depthUpdate` messages captured, 0 non-depth messages.

| Metric | bid `"b"` length | ask `"a"` length |
|--------|-----------------|-----------------|
| min    | 0               | 0               |
| max    | **42**          | **42**          |
| mean   | 3.50            | 3.96            |
| p95    | 21              | 26              |
| p99    | **42**          | **41**          |

Distribution: strongly bimodal — the vast majority of messages have 0–5 entries
per side; isolated bursts to 40+ were observed even within this short window.
p99 = observed max on the bid side, indicating the true tail is not yet sampled.

Unique bid lengths observed: `{0, 1, 2, 3, 4, 5, 11, 20, 21, 35, 39, 40, 42}`  
Unique ask lengths observed: `{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 21, 23, 26, 30, 34, 40, 41, 42}`

### E3 — BTCUSDT exchangeInfo (testnet, 2026-09-13)

Queried via `https://testnet.binance.vision/api/v3/exchangeInfo?symbol=BTCUSDT`.

| Field | Value |
|-------|-------|
| `baseAsset` | BTC |
| `quoteAsset` | USDT |
| `baseAssetPrecision` | 8 |
| `quotePrecision` | 8 |
| `PRICE_FILTER.tickSize` | `0.01000000` (2 decimal places) |
| `PRICE_FILTER.minPrice` | `0.01000000` |
| `PRICE_FILTER.maxPrice` | `1000000.00000000` |
| `LOT_SIZE.stepSize` | `0.00001000` (5 decimal places) |
| `LOT_SIZE.minQty` | `0.00001000` |
| `LOT_SIZE.maxQty` | `9000.00000000` |

**Fixed-point precision analysis**:

| Scale | Price coverage | Qty coverage | Headroom (qty) |
|-------|---------------|--------------|----------------|
| 10^5 (current) | 3 dp headroom | 0 dp headroom | **none — unsafe** |
| 10^7 (proposed) | 5 dp headroom | 2 dp headroom | safe |
| 10^8 | 6 dp headroom | 3 dp headroom | `maxPrice × 10^8 = 10^14` — within `int64_t` max (~9.2 × 10^18) ✓ |

10^7 chosen: eliminates the zero-headroom condition without requiring 10^8,
which would still be within `int64_t` range but provides unnecessary precision
for BTCUSDT specifically.
