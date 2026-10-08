// engine/l2/feature_engine.cpp
// EGBOS — L2 Feature Engine (Transport Skeleton)
//
// Current scope: L1→L2 read path + L2→L3 per-instrument FeatureSlot write.
// No feature mathematics is implemented — no EGBOS feature contract exists.
//
// What this file does:
//   1. Reads the latest valid Top5BidsAsks snapshot from the LOB SHM region.
//   2. Exposes the raw ABI fields (bid_qtys, ask_qtys, bid_price_offsets,
//      ask_price_offsets, anchor_price_low32) for future use — zero processing
//      of price fields is performed here because price reconstruction semantics
//      are unresolved (current unresolved governance decisions §PRICE-RECON).
//   3. Publishes a per-instrument FeatureSlot with:
//        schema_version = kFeatureSlotSchemaVersion (1)
//        validity_mask  = 0 (no approved features)
//        payload        = all zeros
//
// BLOCKED: Price reconstruction (anchor + offset → absolute price).
//          See current unresolved governance decisions §PRICE-RECON.
// BLOCKED: Feature payload population (no EGBOS feature contract exists).
//          See current unresolved governance decisions §FEAT-CONTRACT.

#include "feature_engine.h"
#include <cstdint>

// Placeholder clock — reads CLOCK_MONOTONIC_RAW nanoseconds.
// Isolated in an anonymous namespace so it can be replaced without touching
// the FeatureEngine class.
namespace {
[[maybe_unused]] inline uint64_t monotonic_ns() noexcept {
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
         + static_cast<uint64_t>(ts.tv_nsec);
}
} // anonymous namespace

namespace egbos::l2 {

FeatureEngine::FeatureEngine(
    const egbos::shm::LobSnapshotsRegion* lob_region,
    egbos::shm::FeatureRegion*             feature_region) noexcept
    : reader_(lob_region)
    , publisher_(feature_region)
{}

bool FeatureEngine::process_instrument(uint32_t instrument_id) noexcept {

    // ── Step 1: read L1 snapshot ────────────────────────────────────────────
    // LobSnapshotReader applies the 7-step seqlock protocol.
    // Returns false if region is null, instrument_id is out of range, or
    // the initial sequence is 0 (never published).
    egbos::l1::Top5BidsAsks snapshot{};
    uint64_t source_seq = 0;
    if (!reader_.read(instrument_id, snapshot, source_seq)) {
        return false;
    }

    // ── Step 2: snapshot fields are now available ───────────────────────────
    //
    // Fields available (corrected ABI):
    //   snapshot.bid_qtys[0..4]          — qty-only; safe for research candidates
    //   snapshot.ask_qtys[0..4]          — qty-only; safe for research candidates
    //   snapshot.bid_price_offsets[0..4] — BLOCKED (§PRICE-RECON)
    //   snapshot.ask_price_offsets[0..4] — BLOCKED (§PRICE-RECON)
    //   snapshot.anchor_price_low32      — BLOCKED (§PRICE-RECON)
    //
    // NOTE: candidate functions live in egbos::l2::candidates:: and must NOT
    // be called from this production path until EGBOS approves a feature
    // contract. See engine/l2/candidates/ and §FEAT-CONTRACT.
    (void)snapshot; // suppress unused-variable warning until payload is wired

    // ── Step 3: publish skeleton FeatureSlot ────────────────────────────────
    //
    // validity_mask = 0: no approved features.
    // payload       = nullptr / 0 bytes: no approved feature values.
    // source_seq    = surfaced from L1 snapshot via LobSnapshotReader.
    //
    // When EGBOS approves the first feature contract, this call will be updated
    // to pass the actual payload and validity_mask.
    const uint64_t pub_ts = monotonic_ns();
    return publisher_.publish(
        instrument_id,
        source_seq,
        /* pub_ts_ns     = */ pub_ts,
        /* validity_mask = */ 0u,
        /* feature_count = */ 0u,
        /* src_payload   = */ nullptr,
        /* src_bytes     = */ 0u);
}

} // namespace egbos::l2
