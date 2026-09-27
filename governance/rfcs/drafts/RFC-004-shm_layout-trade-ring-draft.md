# DRAFT — NOT APPLIED — shm_layout.h trade ring extension

**Status: draft for review only. This file is NOT a patch and must NOT be
applied to `engine/core/shm_layout.h` until RFC-004 is accepted and this
draft itself is separately reviewed and approved.**

Related: `governance/rfcs/RFC-004-L0-TRADE-FEED-VPIN-INPUT.yaml`
(status: proposed), specifically `transport.decision:
PROPOSED_SEPARATE_L0_TRADE_RING` and the `shm_init_all_has_no_extension_point`
finding.

This document exists so that, if/when someone with real governance
authority accepts RFC-004's transport decision, the exact mechanical change
to `shm_layout.h` is already spelled out and reviewable, rather than being
improvised at implementation time.

---

## What this draft proposes

Following the existing convention used for every other SHM region in
`shm_layout.h` (see `§1 /dev/shm/egbos_market_events` as the closest
structural analog), add a ninth region: `/dev/shm/egbos_trade_events`.

### 1. New constants block (insert after the existing `§1 market_events` block)

```cpp
// ─────────────────────────────────────────────────────────────────────────────
// §1B  /dev/shm/egbos_trade_events  [L0 → L2]
//      SPSC ring, dedicated trade-event transport (RFC-004).
//      Deliberately SEPARATE from egbos_market_events: TRADE events must
//      never reach L1's book-mutation path (see RFC-004
//      slot_frame_metadata_is_depth_specific_confirmed finding).
//      Slot = RawMarketEvent only (64 bytes) — NOT MarketEventSlot (128B).
//      SlotFrameMetadata (depth-only U/u frame data) has no meaning for a
//      trade event and is intentionally NOT included in this slot.
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr std::size_t kTradeEventsSlots    = /* PENDING */;
inline constexpr std::size_t kTradeEventsSlotSize = sizeof(egbos::events::RawMarketEvent); // 64 bytes
inline constexpr std::size_t kTradeEventsRegionSize =
    ((kTradeEventsSlots * kTradeEventsSlotSize) + kPageSize) & ~(kPageSize - 1);
inline constexpr const char* kTradeEventsPath = "/egbos_trade_events";

static_assert((kTradeEventsSlots & (kTradeEventsSlots - 1)) == 0,
    "kTradeEventsSlots must be a power of 2 — SpscRing uses bitmask modulo");
static_assert(kTradeEventsSlots > 0,
    "kTradeEventsSlots must be non-zero");
static_assert(kTradeEventsSlotSize == 64,
    "Trade event slot is a bare RawMarketEvent (64B), not MarketEventSlot (128B)");
```

**`kTradeEventsSlots` is deliberately left as a non-compiling placeholder
(`/* PENDING */`) rather than a number.** Per RFC-004
`capacity_remains_unsupported`: no throughput/load measurement for the
`<symbol>@trade` stream exists anywhere in this repository. Any concrete
value here must come from empirical measurement, not invention. This is a
deliberate safety property of this draft — it cannot be copy-pasted into
`shm_layout.h` and compiled without someone first supplying and justifying
a real number.

### 2. `ShmRegionPointers` struct — add one field

```cpp
struct ShmRegionPointers {
    void* market_events;
    void* lob_snapshots;
    void* feature_vector;
    void* position_state;
    void* risk_limits;
    void* exec_commands;
    void* regime_signal;
    void* governance_flags;
    void* trade_events;   // NEW — RFC-004, once accepted
};
```

### 3. `shm_init_all()` — add one call

```cpp
    ptrs.trade_events     = open_shm_region(kTradeEventsPath,      kTradeEventsRegionSize,      true);
```

Placement: after `governance_flags` (last), to avoid renumbering/reordering
the existing eight calls and their implicit "canonical order from spec
§6.2" — appending preserves that order rather than inserting into it.

### 4. `kTotalShmBytes` — add one term

```cpp
inline constexpr std::size_t kTotalShmBytes =
    kMarketEventsRegionSize    +
    kLobSnapshotsRegionSize    +
    kFeatureVectorRegionSize   +
    kPositionStateRegionSize   +
    kRiskLimitsRegionSize      +
    kExecCommandsRegionSize    +
    kRegimeSignalRegionSize    +
    kGovernanceFlagsRegionSize +
    kTradeEventsRegionSize;    // NEW — RFC-004, once accepted
```

### 5. Top-of-file region map comment (lines ~84-86 currently list three
regions as an overview; not exhaustive today since eight regions actually
exist, but for consistency a fourth line would be added there too):

```cpp
//   /dev/shm/egbos_trade_events      [L0 → L2]   SPSC ring, dedicated trade transport (RFC-004)
```

---

## What this draft deliberately does NOT decide

- **Capacity** (`kTradeEventsSlots`): left as a placeholder. Requires
  empirical throughput measurement of live `<symbol>@trade` traffic before
  any number is chosen.
- **`instrument_id` scoping**: this draft assumes RFC-004's
  `PROPOSED_BTCUSDT_TO_0_SINGLE_INSTRUMENT` mapping; it does not address
  multi-symbol support.
- **Consumer-side code**: this draft only covers the SHM region definition
  itself. It does NOT specify or imply any L2 consumer implementation,
  the trade-feed listener thread, or the normalizer path that would
  populate this ring. Those remain separate, subsequent implementation
  work, gated on RFC-004 acceptance and this draft's own approval.
- **Whether `kTradeEventsPath` and other names are final** — naming is a
  reviewable detail, not treated as settled by this draft.

## Why this is a draft and not a patch

Modifying `shm_layout.h` changes the ABI backing existing claims (e.g.
`CLAIM-LOB-001`) and was explicitly out of bounds for this analysis
session. This document is offered so the eventual real change is
well-specified and fast to review, not so it can be applied without that
review.

## Review checklist for whoever accepts this

- [ ] RFC-004 itself has moved from `proposed` to `accepted` with a real
      `approval_record` (not `PENDING`).
- [ ] A real value for `kTradeEventsSlots` has been chosen, with its
      empirical basis documented.
- [ ] The exact insertion point and region ordering above has been
      reviewed against the current state of `shm_layout.h` (this draft
      was written against the version at commit `31ce230`; re-diff before
      applying, since the file may have changed since).
- [ ] A corresponding claim (e.g. `CLAIM-L0-TRADE-RING-001`) is drafted
      before or alongside applying this change, per the project's own
      claims-first discipline.
- [ ] `kTotalShmBytes`'s new total is confirmed to still fit the intended
      deployment memory budget.
