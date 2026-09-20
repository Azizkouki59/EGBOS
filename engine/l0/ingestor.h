// engine/l0/ingestor.h
// EGBOS ENGINE_V3 — L0 Feed Ingestor
//
// Specification  : S2-REQ-002, S2-REQ-003, S2-REQ-004 (Gaps, Stale Feeds, NVMe ordering)
// Constraints    : Zero dynamic allocation (GC-E-010). 
//                  Branch prediction bias on hot-path.
//                  Append-only Tier-0 write before SPSC push.
//                  Threshold fsync: 64KB or 5ms, whichever comes first.

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
    
    // Tier-0 append gate: complete write must succeed before SPSC publication.
    [[nodiscard]] bool tier0_append(
        const struct iovec* iov,
        int iovcnt,
        uint64_t now_ns) noexcept;

    // Flush pending Tier-0 bytes when either threshold is reached.
    [[nodiscard]] bool tier0_flush_if_due(uint64_t now_ns) noexcept;

    // Synthetic event emission (Gap / Stale)
    void emit_synthetic_event(uint64_t timestamp_ns, uint8_t event_type) noexcept;

    egbos::shm::MarketEventsRegion* market_events_;
    const egbos::shm::GovernanceFlags* gov_flags_;
    
    int tier0_fd_;

    std::size_t tier0_pending_bytes_;
    uint64_t tier0_last_flush_ns_;

    FeedState current_state_;
    // expected_seq_ removed 2026-09-12: sequence-number gap detection is
    // DEFERRED (S2-REQ-002). The field was orphaned by the Option-A passthrough
    // patch and is no longer read or written anywhere. See ingestor.cpp.
    uint64_t  last_recv_time_ns_;

    // Sticky Tier-0 durability fault flag.
    // Set to true by tier0_flush_if_due() when fsync() fails.
    // NEVER cleared: a hardware/OS storage fault is non-recoverable within a
    // single run.  process_payload() must not reset current_state_ to
    // FEED_NORMAL while this flag is set, so a Tier-0 failure cannot be
    // silently overwritten by the next arriving frame.
    bool tier0_fault_{false};
    
    // Tier-0 durability thresholds per AD-003 / S2.
    static constexpr std::size_t kTier0FlushBytes = 64 * 1024;
    static constexpr uint64_t kTier0FlushIntervalNs = 5'000'000ULL;

    // Feed staleness threshold per REG-FEED-STALE-001.
    static constexpr uint64_t kStaleThresholdNs = 500'000'000ULL;
};

} // namespace egbos::l0
