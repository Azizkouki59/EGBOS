// engine/l2/feature_engine.cpp
// EGBOS — L2 Feature Engine (Transport Skeleton)
//
// Current scope: L1→L2 read path + L2→L3 write skeleton.
// No feature mathematics is implemented — no EGBOS feature contract exists.
//
// What this file does:
//   1. Reads the latest valid Top5BidsAsks snapshot from the LOB SHM region.
//   2. Exposes the raw ABI fields (bid_qtys, ask_qtys, bid_price_offsets,
//      ask_price_offsets, anchor_price_low32) for future use — zero processing
//      of price fields is performed here because price reconstruction semantics
//      are unresolved (current unresolved governance decisions §PRICE-RECON).
//   3. Writes a schema-versioned FeatureVector with validity_mask=0 (no
//      approved features) and a zeroed feature payload.
//
// BLOCKED: Price reconstruction (anchor + offset → absolute price).
//          See current unresolved governance decisions §PRICE-RECON.
// BLOCKED: Feature payload population (no EGBOS feature contract exists).
//          See current unresolved governance decisions §FEAT-CONTRACT.
// BLOCKED: L2→L3 transport architecture (A/B/C not yet chosen).
//          See current unresolved governance decisions §L2L3-ARCH.

#include "feature_engine.h"
#include <cstring>

namespace egbos::l2 {

FeatureEngine::FeatureEngine(
    const egbos::shm::LobSnapshotsRegion* lob_region,
    egbos::shm::FeatureVectorRegion* feature_region) noexcept
    : reader_(lob_region), feature_region_(feature_region) {}

bool FeatureEngine::process_instrument(uint32_t instrument_id) noexcept {
    if (!feature_region_) {
        return false;
    }

    // ── Step 1: read L1 snapshot ────────────────────────────────────────────
    // LobSnapshotReader applies the 7-step seqlock protocol:
    //   (1) acquire active buffer index
    //   (2) acquire sequence from that buffer
    //   (3) reject odd sequence (write in progress — retry)
    //   (4) copy exactly 64 defined payload bytes
    //   (5) acquire fence
    //   (6) re-check sequence — must equal seq from step 2
    //   (7) re-check active buffer — must equal buffer from step 1
    // Returns false if region is null, instrument_id is out of range, or
    // the initial sequence is 0 (never published).
    egbos::l1::Top5BidsAsks snapshot{};
    if (!reader_.read(instrument_id, snapshot)) {
        return false;
    }

    // ── Step 2: snapshot fields are now available ───────────────────────────
    //
    // Fields available (repository ABI/schema contract corrected ABI):
    //   snapshot.bid_qtys[0..4]          — bid quantities, level 0 = best bid
    //   snapshot.ask_qtys[0..4]          — ask quantities, level 0 = best ask
    //   snapshot.bid_price_offsets[0..4] — signed int16 offsets from anchor
    //   snapshot.ask_price_offsets[0..4] — signed int16 offsets from anchor
    //   snapshot.anchor_price_low32      — low 32 bits of anchor price
    //
    // BLOCKED: price reconstruction from anchor + offsets.
    //   The unit/scale of anchor_price_low32 is unresolved.
    //   The mapping from int16 offset to price delta is unresolved.
    //   The range/overflow guarantee of low32 truncation is unresolved.
    //   See current unresolved governance decisions §PRICE-RECON.
    //   Any attempt to compute MID_1, SPREAD_1, WEIGHTED_MID_1, or any
    //   price-distance-weighted quantity from these fields is PROHIBITED
    //   until current unresolved governance decisions §PRICE-RECON is resolved.
    //
    // Fields that ARE safe to pass to candidate research functions (qty-only):
    //   snapshot.bid_qtys[]  — no price reconstruction required
    //   snapshot.ask_qtys[]  — no price reconstruction required
    //
    // NOTE: candidate functions live in egbos::l2::candidates:: and must NOT
    // be called from this production path until EGBOS approves a feature
    // contract. See engine/l2/candidates/ and current unresolved governance decisions §FEAT-CONTRACT.

    // ── Step 3: build skeleton FeatureVector ────────────────────────────────
    //
    // PROVISIONAL LAYOUT — NOT EGBOS-APPROVED, see spec §10.
    // validity_mask = 0: no approved features exist yet.
    // feature_payload = all zeros: no approved feature values.
    //
    // When EGBOS approves the first feature contract, that contract will:
    //   (a) assign a feature_id (bit index in validity_mask_lo64)
    //   (b) define which bytes in feature_payload carry the feature value
    //   (c) define the exact formula and wire it into this function
    // Until then, this block must not be changed to populate any payload bytes.
    alignas(64) uint8_t feature_data[egbos::shm::kFeatureVectorBytes]{};
    build_skeleton_vector(feature_data, /* validity_mask = */ 0u);

    // ── Step 4: publish to L3 ───────────────────────────────────────────────
    feature_region_->write(feature_data);

    return true;
}

} // namespace egbos::l2
