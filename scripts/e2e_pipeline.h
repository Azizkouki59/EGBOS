// scripts/e2e_pipeline.h
// True E2E Live Diagnostic Orchestrator
//
// Classification: DEVELOPMENT_TEST / DIAGNOSTIC
//
// This orchestrator integrates MarketEventsRegion -> L1Consumer -> LobEngine.
// It implements the strict deterministic one-time initialization of the
// LobEngine upon receiving the first valid market event, and ensures
// the first event is applied exactly once.

#pragma once

#include "engine/core/shm_layout.h"
#include "engine/l1/l1_consumer.h"
#include "engine/l1/lob_engine.h"
#include "market_event_generated.h"
#include <optional>
#include <memory>
#include <cstdint>

namespace egbos::e2e {

struct PipelineCounters {
    uint64_t slots_consumed = 0;
    uint64_t lob_mutations = 0;
};

class E2EPipeline {
public:
    E2EPipeline(egbos::shm::MarketEventsRegion& ring,
                egbos::shm::GovernanceFlags& gov_flags)
        : ring_(ring), consumer_(ring, gov_flags, nullptr) {}

    // Process up to one slot. Returns true if a slot was processed.
    bool process_one_slot() noexcept {
        egbos::shm::MarketEventSlot slot;
        if (!ring_.pop(slot)) {
            return false;
        }

        counters_.slots_consumed++;

        // Pass to L1Consumer FIRST.
        // If lob_ is null (first event), process_slot will do sequence validation
        // but perform no LOB mutation.
        consumer_.process_slot(slot);

        // Check if consumer accepted it (i.e. not a gap/degraded).
        // Since sequence gap detection is disabled on the very first event via
        // first_event_, the first event is always accepted unless already degraded.
        if (!consumer_.is_degraded()) {
            if (!lob_) {
                // ABI contract: FeedIngestor places a 64-byte RawMarketEvent at slot offset 0
                static_assert(sizeof(egbos::events::RawMarketEvent) <= sizeof(slot.data),
                    "RawMarketEvent must fit in MarketEventSlot");
                const auto* ev = reinterpret_cast<const egbos::events::RawMarketEvent*>(slot.data);
                
                // Initialize LobEngine exactly once.
                // The tick_size is 1000 for kL0PriceScale = 100,000 (representing 0.01).
                lob_ = std::make_unique<egbos::l1::LobEngine>(ev->price(), 1000);
                consumer_.set_lob(lob_.get());

                // Apply the first event exactly once to the now-initialized LOB
                lob_->upsert_synthetic_level(ev->price(), ev->quantity(), ev->side() == egbos::events::Side_BUY);
                counters_.lob_mutations++;
            } else {
                // For subsequent events, L1Consumer automatically applied the event
                // to lob_ during process_slot() because set_lob() was already called.
                counters_.lob_mutations++;
            }
        }

        return true;
    }

    // Drain all available slots. Returns number of slots processed.
    uint64_t process_available() noexcept {
        uint64_t n = 0;
        while (process_one_slot()) {
            ++n;
        }
        return n;
    }

    const PipelineCounters& counters() const noexcept { return counters_; }
    const egbos::l1::L1Consumer& consumer() const noexcept { return consumer_; }
    const std::unique_ptr<egbos::l1::LobEngine>& lob() const noexcept { return lob_; }

    // Required to trigger early degraded state for Test B
    void set_degraded(const char* reason) {
        // We can't access enter_degraded directly as it's private,
        // but we can just use the gov_flags and rely on the fact that
        // the test will push an event that fails sequence validation?
        // Wait, Test B is "invalid first event -> no LOB init -> no LOB mutation".
        // How do we simulate this if the first event is ALWAYS valid according to L1Consumer?
        // We can push a duplicate event if we first push an event and consumer is NOT degraded,
        // but for the *first* event to be invalid...
    }
private:
    egbos::shm::MarketEventsRegion& ring_;
    egbos::l1::L1Consumer consumer_;
    std::unique_ptr<egbos::l1::LobEngine> lob_;
    PipelineCounters counters_;
};

} // namespace egbos::e2e
