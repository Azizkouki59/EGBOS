// engine/l0/ingestor.h
// EGBOS ENGINE_V3 — L0 Feed Ingestor
//
// Specification  : S2-REQ-002, S2-REQ-003, S2-REQ-004 (Gaps, Stale Feeds, NVMe ordering)
// Constraints    : Zero dynamic allocation (GC-E-010). 
//                  Branch prediction bias on hot-path.
//                  Append-only Tier-0 fsync via writev before SPSC push.

#pragma once

#include <cstdint>
#include <cstddef>
#if defined(_WIN32)
// DEVELOPMENT/TEST ONLY — NOT TIER-0
struct iovec {
    void  *iov_base;
    size_t iov_len;
};
typedef int ssize_t;
#else
#include <sys/uio.h> // for iovec
#endif
#include "engine/core/shm_layout.h"
#include "engine/l0/binance_json_normalizer.h" // also pulls in market_event_generated.h

namespace egbos::l0 {

enum class FeedState : uint8_t {
    FEED_NORMAL = 0,
    GAP_DETECTED = 1,
    FEED_STALE = 2
};

class FeedIngestor {
public:
    // Configures the ingestor with SHM boundaries and Tier-0 log FD.
    // Does NOT allocate heap memory.
    FeedIngestor(
        egbos::shm::MarketEventsRegion* market_events,
        const egbos::shm::GovernanceFlags* gov_flags,
        int tier0_fd) noexcept;

    ~FeedIngestor() = default;

    // Hot-path entry point. Network boundary agnostic (UDP/TCP payload).
    // Parses exchange protocol, normalizes to RawMarketEvent, writes to Tier-0, pushes to SPSC.
    void process_payload(const uint8_t* payload, std::size_t length, uint64_t rx_timestamp_ns) noexcept;

    // Heartbeat check invoked by the polling loop if no packets arrive.
    // Transitions to FEED_STALE if threshold exceeded.
    void check_staleness(uint64_t current_time_ns) noexcept;

private:
    // Hot-path parsing and normalization.
    void normalize_and_forward(const uint8_t* event_data, std::size_t event_len, uint64_t rx_timestamp_ns) noexcept;
    
    // Tier-0 NVMe durability using writev for zero-copy coalescing.
    void tier0_append(const struct iovec* iov, int iovcnt) noexcept;

    // Synthetic event emission (Gap / Stale)
    void emit_synthetic_event(uint64_t timestamp_ns, uint8_t event_type) noexcept;

    egbos::shm::MarketEventsRegion* market_events_;
    const egbos::shm::GovernanceFlags* gov_flags_;
    
    int tier0_fd_;
    
    FeedState current_state_;
    // expected_seq_ removed 2026-09-12: sequence-number gap detection is
    // DEFERRED (S2-REQ-002). The field was orphaned by the Option-A passthrough
    // patch and is no longer read or written anywhere. See ingestor.cpp.
    uint64_t  last_recv_time_ns_;
    
    // Configurable thresholds (normally from spec/ADRs)
    static constexpr uint64_t kStaleThresholdNs = 500'000'000; // 500ms
};

} // namespace egbos::l0
