// engine/l2/feature_engine.cpp
// EGBOS — L2 FeatureEngine (production framework, contract-driven)
//
// Implements the per-instrument production pipeline:
//   read L1 snapshot → validate → dedup → compute_production_features() → publish slot
//
// RESEARCH ISOLATION GUARANTEE
//   This file MUST NOT include any header from engine/l2/candidates/.
//   It MUST NOT include hawkes_intensity.h or any research-only module.
//   The only computation entry point is compute_production_features()
//   declared in engine/l2/feature_compute.h.
//
// PRODUCTION STATE (as of FEAT-0001 ratification)
//   compute_production_features() returns validity_mask=1, feature_count=1,
//   payload_bytes=4 with FEAT-0001 (Hawkes Intensity) in bytes 0-3.
//   Governance claim: governance/claims/FEAT-0001-Hawkes-Intensity.md
//   Parameter registry: governance/claims/PARAMETER_REGISTRY
//
// §PRICE-RECON RESOLVED: anchor_price (int64_t) and tick_size (int64_t) are now
//   in the Top5BidsAsks ABI (v2). Use snap.calculate_absolute_price(offset).

#include "l2/feature_engine.h"
#include "l2/feature_compute.h"
// IMPORTANT: Do NOT include any header from engine/l2/candidates/ here.

#include <cstdint>
#include <ctime>
#include <cstring>
#include <time.h>    // clock_gettime, CLOCK_MONOTONIC_RAW

namespace egbos::l2 {

// ── FeatureEngine::now_monotonic_ns ────────────────────────────────────────
//
// Isolated clock query.  Using CLOCK_MONOTONIC_RAW avoids NTP adjustments.
// Called exactly once per process_instrument() tick.
// The result is passed into compute_production_features() as pub_ts_ns so
// that the compute layer never queries the clock directly (GC-E-011 / clock policy).
/*static*/ uint64_t FeatureEngine::now_monotonic_ns() noexcept {
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
         + static_cast<uint64_t>(ts.tv_nsec);
}

// ── FeatureEngine::FeatureEngine ────────────────────────────────────────────
FeatureEngine::FeatureEngine(
    const egbos::shm::LobSnapshotsRegion* lob_region,
    egbos::shm::FeatureRegion*             feature_region) noexcept
    : reader_(lob_region)
    , publisher_(feature_region)
{
    // Zero-initialise per-instrument last_source_seq dedup table.
    std::memset(last_source_seq_, 0, sizeof(last_source_seq_));
}

// ── FeatureEngine::process_instrument ──────────────────────────────────────
bool FeatureEngine::process_instrument(uint32_t instrument_id) noexcept {

    // ── Step 1: validate instrument range ────────────────────────────────
    if (instrument_id >= egbos::shm::kMaxInstruments) {
        return false;
    }

    // ── Step 2: read L1 snapshot via seqlock protocol ────────────────────
    // LobSnapshotReader::read() applies the seqlock protocol and surfaces the
    // validated L1 sequence number.  Returns false if:
    //   - lob_region is null
    //   - instrument_id is out of range
    //   - the L1 slot has never been published (seq == 0)
    egbos::l1::Top5BidsAsks snapshot{};
    uint64_t source_seq = 0u;
    if (!reader_.read(instrument_id, snapshot, source_seq)) {
        return false;
    }

    // ── Step 3: validate source sequence ─────────────────────────────────
    if (source_seq == 0u) {
        return false;
    }

    // ── Step 4: deduplication — skip if L1 sequence unchanged ────────────
    // Avoids redundant feature re-computation and re-publication when the
    // FeatureEngine sweeps faster than L1 publishes.
    const uint64_t prev_seq = last_source_seq_[instrument_id];
    if (source_seq == prev_seq) {
        return false;
    }

    // ── Step 5: gap detection → reset Hawkes state ────────────────────────
    // A sequence gap (source_seq jumped by more than 1 publish) implies that
    // at least one LOB snapshot was missed.  Per RFC-003 §proposal_hawkes
    // "reset behavior on GapEvent: Hawkes state resets to baseline on gap",
    // reset the per-instrument feature state so the next compute starts fresh.
    //
    // Gap definition: prev_seq > 0 and source_seq > prev_seq + 2.
    // (Each L1 publish increments the seqlock counter by 2: odd during write,
    // even on completion; consecutive publishes on alternating buffers may share
    // seq values, so we conservatively treat a jump > 2 as a gap.)
    if (prev_seq > 0u && source_seq > prev_seq + 2u) {
        reset_production_features(instrument_id);
    }

    // Update dedup table.
    last_source_seq_[instrument_id] = source_seq;

    // ── Step 6: query timestamp (once per tick) ───────────────────────────
    // This is the SOLE clock query on the hot path.  The timestamp is forwarded
    // to compute_production_features() so that the compute layer stays clock-free.
    const uint64_t pub_ts = now_monotonic_ns();

    // ── Step 7: invoke production feature computation ─────────────────────
    //
    // compute_production_features() is the SOLE production entry point.
    // It is implemented in feature_compute.cpp which MUST NOT include any
    // research candidate module.
    FeatureComputeOutput compute_out{};  // zero-initialized
    if (!compute_production_features(instrument_id, snapshot, pub_ts, compute_out)) {
        return false;
    }

    // ── Step 8: payload size guard ────────────────────────────────────────
    // Defensive check before publish: compute_production_features() must not
    // produce a payload larger than the fixed FeatureSlot capacity.
    // FeaturePublisher::publish() also checks this, but we gate it here for
    // an explicit, auditable validation layer.
    if (compute_out.payload_bytes > egbos::shm::kFeatureSlotPayloadBytes) {
        return false;
    }

    // ── Step 9: publish per-instrument FeatureSlot ───────────────────────
    return publisher_.publish(
        instrument_id,
        /* source_seq    = */ source_seq,
        /* pub_ts_ns     = */ pub_ts,
        /* validity_mask = */ compute_out.validity_mask,
        /* feature_count = */ compute_out.feature_count,
        /* src_payload   = */ compute_out.payload_bytes > 0u
                                ? compute_out.payload
                                : nullptr,
        /* src_bytes     = */ compute_out.payload_bytes);
}

// ── FeatureEngine::process_all_published ───────────────────────────────────
uint32_t FeatureEngine::process_all_published() noexcept {
    uint32_t success_count = 0u;
    for (uint32_t id = 0u; id < egbos::shm::kMaxInstruments; ++id) {
        // process_instrument returns false for unpublished instruments (seq==0)
        // or unchanged instruments (dedup).  Both are normal and not failures.
        if (process_instrument(id)) {
            ++success_count;
        }
    }
    return success_count;
}

} // namespace egbos::l2
