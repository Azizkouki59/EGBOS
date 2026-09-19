// engine/l1/l1_consumer.h
// EGBOS — L1 Consumer Loop
//
// Polls MarketEventsRegion, reads a RawMarketEvent from each slot (produced
// by FeedIngestor::normalize_and_forward via binance_json_normalizer), performs
// per-event sequence-gap detection and routes to the LOB engine.
//
// S2-REQ-001 (F3): L1 no longer parses raw JSON from slots. The JSON
// normalization pipeline now lives entirely in L0 (binance_json_normalizer).
// L1 reads the 64-byte RawMarketEvent placed at slot.data[0] by L0.
//
// Classification  : ARCHITECTURE / INTEGRATION
// Provenance      : NOT LIVE_OPERATIONAL — does not constitute S5 evidence.
// Constraints     : No heap allocation in the consume() hot path (GC-E-010).

#pragma once

#include "market_event_generated.h"
#include "engine/core/shm_layout.h"
#include "engine/l1/lob_engine.h"

#include <cstdint>
#include <cstdio>

namespace egbos::l1 {

// ── EventCounts ───────────────────────────────────────────────────────────────
// Counters for events observed by the L1Consumer.  Replaces ParseResultCounts
// now that JSON parsing has moved to L0.
struct EventCounts {
    uint64_t ok                      = 0;  // RawMarketEvent processed successfully
    uint64_t sequence_gap_detected   = 0;  // genuine inter-frame gap (U continuity break)
    uint64_t skipped_while_degraded  = 0;  // slot discarded after DEGRADED latch
    uint64_t same_frame_levels       = 0;  // slots sharing final_update_id with prev (same frame)
};

// ── L1Consumer ────────────────────────────────────────────────────────────────
// Stateful consumer for MarketEventsRegion.
//
// Usage:
//   L1Consumer consumer(market_events_region, gov_flags);
//   while (running) {
//       consumer.consume_one();  // or consume_available()
//   }
//   consumer.print_summary();
class L1Consumer {
public:
    explicit L1Consumer(shm::MarketEventsRegion& ring,
                        shm::GovernanceFlags&    gov_flags,
                        LobEngine*               lob = nullptr) noexcept
        : ring_(ring), gov_flags_(gov_flags), lob_(lob) {}

    void set_lob(LobEngine* lob) noexcept { lob_ = lob; }

    // consume_one: pop one slot and process it.
    // Returns true if a slot was available and consumed, false if ring was empty.
    bool consume_one() noexcept;

    // consume_available: drain all currently available slots.
    // Returns the number of slots consumed in this call.
    uint64_t consume_available() noexcept;

    // print_summary: write counters to stdout.
    // Safe to call from any point — no allocation.
    void print_summary() const noexcept;

    // Accessors for test verification.
    const EventCounts& counts()          const noexcept { return counts_; }
    uint64_t           slots_popped()    const noexcept { return slots_popped_; }
    bool               is_degraded()     const noexcept { return degraded_; }
    uint64_t           expected_next()   const noexcept { return last_final_u_ + 1; }

    // Reset counters and gap-tracking state (useful for test isolation).
    void reset() noexcept;

    // Process a single slot directly (useful for runners that intercept events).
    void process_slot(const shm::MarketEventSlot& slot) noexcept;

private:
    shm::MarketEventsRegion& ring_;
    shm::GovernanceFlags&    gov_flags_;
    LobEngine*               lob_ = nullptr;

    EventCounts counts_{};
    uint64_t    slots_popped_         = 0;
    bool        first_event_          = true;
    // Frame-level gap detection state (replaces per-level sequence_num check).
    // last_final_u_: the Binance u (final_update_id) of the most recently
    //   processed frame.  A new slot from the same frame has
    //   meta.final_update_id == last_final_u_.  A new frame must satisfy
    //   meta.first_update_id == last_final_u_ + 1 (Binance continuity rule).
    uint64_t    last_final_u_         = 0;
    uint64_t    last_first_u_         = 0;
    bool        degraded_             = false;

    void enter_degraded(const char* reason) noexcept;
};

} // namespace egbos::l1
