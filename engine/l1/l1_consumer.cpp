// engine/l1/l1_consumer.cpp
// EGBOS — L1 Consumer Loop implementation
//
// S2-REQ-001 (F3): process_slot now reads a RawMarketEvent struct placed at
// slot.data[0] by FeedIngestor::normalize_and_forward.  All JSON parsing has
// moved to L0 (binance_json_normalizer).  Sequence gap detection uses
// RawMarketEvent::sequence_num() instead of ParsedDepthUpdate::first_update_id.

#include "l1_consumer.h"

#include <cstdio>
#include <cstring>
#include <cinttypes>

namespace egbos::l1 {

// ── enter_degraded ────────────────────────────────────────────────────────────
void L1Consumer::enter_degraded(const char* reason) noexcept {
    if (!degraded_) {
        std::fprintf(stderr,
            "[L1Consumer] DEGRADED: %s — setting feed_stale, halting LOB updates.\n"
            "  Snapshot recovery is not yet designed; this state is terminal until restart.\n",
            reason);
    }
    degraded_ = true;
    gov_flags_.feed_stale.store(true, std::memory_order_release);
    // NOTE: we do NOT set halt_required here — that flag is written by the
    // governance subsystem, not directly by L1. We only set feed_stale and
    // stop mutating the LOB.
}

// ── process_slot ─────────────────────────────────────────────────────────────
void L1Consumer::process_slot(const shm::MarketEventSlot& slot) noexcept {
    if (degraded_) {
        counts_.skipped_while_degraded++;
        return;
    }

    // Reinterpret slot.data as a RawMarketEvent (64 bytes at offset 0).
    // The slot is 128 bytes; RawMarketEvent occupies the first 64.
    static_assert(sizeof(egbos::events::RawMarketEvent) <= sizeof(slot.data),
        "RawMarketEvent does not fit in MarketEventSlot");
    const auto* event = reinterpret_cast<const egbos::events::RawMarketEvent*>(
        slot.data);

    // Read SlotFrameMetadata from bytes 64-127.
    // Written by FeedIngestor::normalize_and_forward with Binance U/u values.
    const shm::SlotFrameMetadata& meta = shm::slot_frame_meta(slot);

    // ── Frame-level gap detection (S2-REQ-002) ────────────────────────────────
    // The Binance @depth feed emits one slot per price level; all levels from
    // the same frame share the same final_update_id (u == meta.final_update_id).
    //
    // Same-frame check: meta.final_update_id == last_final_u_
    //   → continuation of the current frame, no gap validation needed.
    //
    // New-frame check: meta.final_update_id != last_final_u_
    //   Valid continuation:   meta.first_update_id == last_final_u_ + 1
    //   Genuine gap:          meta.first_update_id != last_final_u_ + 1
    //
    // On the very first event, last_final_u_==0 and first_event_==true;
    // we skip the gap check and accept the frame unconditionally.
    if (!first_event_) {
        const bool same_frame = (meta.final_update_id == last_final_u_) && 
                                (meta.first_update_id == last_first_u_);
        if (same_frame) {
            // Same Binance frame — not a gap.  All levels are applied.
            counts_.same_frame_levels++;
        } else {
            // New frame: verify U continuity.
            // Binance rule: frame.U == prev_frame.u + 1
            if (meta.first_update_id != last_final_u_ + 1) {
                counts_.sequence_gap_detected++;
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "frame gap: prev_u=%" PRIu64 " new_U=%" PRIu64
                    " new_u=%" PRIu64 " (expected new_U=%" PRIu64 ")",
                    last_final_u_,
                    meta.first_update_id,
                    meta.final_update_id,
                    last_final_u_ + 1);
                enter_degraded(buf);
                return; // do NOT apply gapped frame's events to LOB
            }
        }
    }

    // Frame is valid (same frame continuation, or valid new frame, or first event).
    first_event_    = false;
    last_final_u_   = meta.final_update_id;
    last_first_u_   = meta.first_update_id;
    counts_.ok++;

    // LOB integration: route the event to the limit order book.
    if (lob_) {
        lob_->upsert_synthetic_level(event->price(), event->quantity(),
                                     event->side() == egbos::events::Side_BUY);
    }
}

// ── consume_one ───────────────────────────────────────────────────────────────
bool L1Consumer::consume_one() noexcept {
    shm::MarketEventSlot slot;
    if (!ring_.pop(slot)) {
        return false;
    }
    slots_popped_++;
    process_slot(slot);
    return true;
}

// ── consume_available ─────────────────────────────────────────────────────────
uint64_t L1Consumer::consume_available() noexcept {
    uint64_t n = 0;
    shm::MarketEventSlot slot;
    while (ring_.pop(slot)) {
        slots_popped_++;
        process_slot(slot);
        ++n;
    }
    return n;
}

// ── print_summary ─────────────────────────────────────────────────────────────
void L1Consumer::print_summary() const noexcept {
    std::fprintf(stdout,
        "\n──────────────────────────────────────────────\n"
        " L1Consumer summary\n"
        "──────────────────────────────────────────────\n"
        " Slots popped            : %" PRIu64 "\n"
        " Skipped (while DEGRADED): %" PRIu64 "\n"
        " OK (processed, no gap)  : %" PRIu64 "\n"
        " Same-frame levels       : %" PRIu64 "\n"
        " Sequence gaps detected  : %" PRIu64 "\n"
        " State                   : %s\n"
        "──────────────────────────────────────────────\n",
        slots_popped_,
        counts_.skipped_while_degraded,
        counts_.ok,
        counts_.same_frame_levels,
        counts_.sequence_gap_detected,
        degraded_ ? "DEGRADED" : "NOMINAL"
    );
}

// ── reset ─────────────────────────────────────────────────────────────────────
void L1Consumer::reset() noexcept {
    counts_            = EventCounts{};
    slots_popped_      = 0;
    first_event_       = true;
    last_final_u_      = 0;
    last_first_u_      = 0;
    degraded_          = false;
    // Do NOT reset gov_flags_ — those are a shared region; this consumer
    // cannot unilaterally clear them.
}

} // namespace egbos::l1
