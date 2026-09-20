// engine/l0/ingestor.cpp
// EGBOS ENGINE_V3 — L0 Feed Ingestor Implementation
//
// Claims         : S2-REQ-002, S2-REQ-003, S2-REQ-004
// Path           : engine/l0/ingestor.cpp

#include "ingestor.h"
#include <cstring>
#if defined(_WIN32)
// DEVELOPMENT/TEST ONLY — NOT TIER-0
#include <io.h>
inline ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        int written = _write(fd, iov[i].iov_base, static_cast<unsigned int>(iov[i].iov_len));
        if (written < 0) return -1;
        total += written;
    }
    return total;
}
#else
#include <sys/uio.h>
#include <unistd.h>
#endif

namespace egbos::l0 {

FeedIngestor::FeedIngestor(
    egbos::shm::MarketEventsRegion* market_events,
    const egbos::shm::GovernanceFlags* gov_flags,
    int tier0_fd) noexcept
    : market_events_(market_events),
      gov_flags_(gov_flags),
      tier0_fd_(tier0_fd),
      tier0_pending_bytes_(0),
      tier0_last_flush_ns_(0),
      current_state_(FeedState::FEED_NORMAL),
      last_recv_time_ns_(0) {}


void FeedIngestor::process_payload(const uint8_t* payload, std::size_t length, uint64_t rx_timestamp_ns) noexcept {
    // OPTION-A PASSTHROUGH (2026-09-12):
    // The Binance JSON WebSocket protocol does NOT prefix frames with an
    // 8-byte binary sequence number. The previous stub was silently stripping
    // the first 8 bytes of every JSON message (e.g. consuming "depthUpd"
    // from "{\"e\":\"depthUpdate\",...}") and reinterpreting those bytes as a
    // sequence number, causing every real message to be misclassified as a
    // GAP_DETECTED event (audit Finding 7).
    //
    // Sequence-number extraction and gap detection for the Binance JSON feed
    // require a proper JSON parser to extract U/u update IDs; that is deferred
    // to a future ADR. Until then:
    //   - All arriving frames are passed to normalize_and_forward() intact.
    //   - expected_seq_ is not used; gap detection is disabled on this path.
    //   - Stale detection (check_staleness) continues to operate correctly.
    //
    // S2-REQ-002 compliance: DEFERRED — gap detection is stubbed to passthrough.
    // This function must be revisited when a zero-allocation JSON parser is
    // approved as a dependency.

    if (length == 0) [[unlikely]] {
        return;
    }

    // D4: Record CLOCK_MONOTONIC_RAW timestamp injected by the io_uring CQE caller.
    last_recv_time_ns_ = rx_timestamp_ns;

    // Transition back to FEED_NORMAL if we were in another state — but ONLY
    // when there is no sticky Tier-0 durability fault.  A Tier-0 fault is set
    // by tier0_flush_if_due() on fsync() failure and is never cleared within
    // a run; it must not be silently overwritten by the next arriving frame.
    if (current_state_ != FeedState::FEED_NORMAL && !tier0_fault_) [[unlikely]] {
        current_state_ = FeedState::FEED_NORMAL;
        // S1 SPEC CORRECTION: L0 is execution plane. It cannot write to gov_flags_.
        // Governance subsystem monitors Tier-0 and sets gov_flags_ out-of-band.
    }

    // Forward the complete, unstripped payload.
    normalize_and_forward(payload, length, rx_timestamp_ns);
}

void FeedIngestor::check_staleness(uint64_t current_time_ns) noexcept {
    // AD-003: time-based Tier-0 flush must also progress while the feed
    // is idle, not only when another market-data frame arrives.
    if (!tier0_flush_if_due(current_time_ns)) {
        // A Tier-0 durability failure must not permit further downstream
        // publication. Preserve the fail-safe feed-stale state.
        current_state_ = FeedState::FEED_STALE;
        return;
    }

    if (last_recv_time_ns_ == 0) [[unlikely]] return;

    if (current_state_ != FeedState::FEED_STALE) [[likely]] {
        const uint64_t idle_ns =
            current_time_ns >= last_recv_time_ns_
                ? (current_time_ns - last_recv_time_ns_)
                : 0;

        // Temporary diagnostics: expose the exact local idle age approaching
        // the 500ms stale boundary.
        if (idle_ns >= 490'000'000ULL) {
            std::fprintf(
                stderr,
                "[STALE-DIAG] idle_us=%llu last_recv_ns=%llu now_ns=%llu pending=%zu\n",
                static_cast<unsigned long long>(idle_ns / 1'000ULL),
                static_cast<unsigned long long>(last_recv_time_ns_),
                static_cast<unsigned long long>(current_time_ns),
                tier0_pending_bytes_);
        }

        if (idle_ns > kStaleThresholdNs) [[unlikely]] {
            current_state_ = FeedState::FEED_STALE;
            // S2-REQ-004: Emit FeedStaleEvent.
            // S1 SPEC CORRECTION: Do NOT write to gov_flags_ (read-only for execution plane).
            emit_synthetic_event(current_time_ns, 2); // 2 = STALE type
        }
    }
}

void FeedIngestor::normalize_and_forward(const uint8_t* event_data, std::size_t event_len, uint64_t rx_timestamp_ns) noexcept {
    // S2-REQ-001 normalization: parse the Binance @depth JSON frame and emit
    // one RawMarketEvent slot per price level into the SPSC ring.
    //
    // F1 fix: raw JSON bytes are no longer copied into slot.data.
    // F2 fix: rx_timestamp_ns is stored in RawMarketEvent::ingestion_tsc_ via
    //         the struct constructor — not via a raw memcpy to offset 24.

    struct CallbackCtx {
        FeedIngestor* self;
        uint64_t      rx_timestamp_ns; // passed through to validate
    };
    CallbackCtx ctx{ this, rx_timestamp_ns };

    auto callback = [](const egbos::events::RawMarketEvent& event,
                       uint64_t first_update_id,
                       uint32_t frame_level_index,
                       void* user_ctx) noexcept -> bool
    {
        auto* c = static_cast<CallbackCtx*>(user_ctx);
        FeedIngestor* ingestor = c->self;

        // Construct the SPSC slot.
        // Bytes 0-63:  RawMarketEvent (FlatBuffers struct, 64 bytes).
        // Bytes 64-127: SlotFrameMetadata (U/u frame identity, 64 bytes).
        egbos::shm::MarketEventSlot slot;
        std::memset(slot.data, 0, sizeof(slot.data));

        // Copy the 64-byte FlatBuffers struct into bytes 0-63.
        static_assert(sizeof(egbos::events::RawMarketEvent) <= 64,
            "RawMarketEvent does not fit in lower half of MarketEventSlot");
        std::memcpy(slot.data, &event, sizeof(egbos::events::RawMarketEvent));

        // Write SlotFrameMetadata into bytes 64-127.
        // This carries U (first_update_id) and u (final_update_id = sequence_num)
        // so L1Consumer can perform correct frame-level gap detection.
        egbos::shm::SlotFrameMetadata& meta = egbos::shm::slot_frame_meta(slot);
        meta.first_update_id   = first_update_id;
        meta.final_update_id   = event.sequence_num();  // == u (final_update_id)
        meta.frame_level_index = frame_level_index;
        // _reserved is already zero from the memset above.

        // Tier-0 Durability Logging (S2-REQ-004)
        // NVMe ordering: append to Tier-0 BEFORE pushing to SPSC.
        struct iovec iov[1];
        iov[0].iov_base = slot.data;
        iov[0].iov_len  = sizeof(slot.data);
        // Tier-0 is the ordering gate: no downstream publication is allowed
        // unless the complete 128-byte append succeeded.
        if (!ingestor->tier0_append(iov, 1, c->rx_timestamp_ns)) {
            return false; // abort this frame; do NOT push to SPSC
        }

        // Hand over to L1 only after a complete Tier-0 append.
        if (ingestor->market_events_) {
            ingestor->market_events_->push(slot);
        }
        return true; // continue processing further levels
    };

    L0ParseStatus status = normalize_binance_depth(
        event_data, event_len, rx_timestamp_ns, callback, &ctx);

    // Parse failures are silently discarded at L0; the governance subsystem
    // monitors Tier-0 for anomalies and can assert DEGRADED out-of-band.
    // No heap allocation takes place on this path (GC-E-010).
    (void)status;
}

void FeedIngestor::emit_synthetic_event(uint64_t timestamp_ns, uint8_t event_type) noexcept {
    egbos::shm::MarketEventSlot slot;
    std::memset(slot.data, 0, sizeof(slot.data));
    
    // Stub encoding for synthetic events (Gap, Stale)
    slot.data[0] = 0xFF; // Synthetic marker
    slot.data[1] = event_type;
    std::memcpy(&slot.data[2], &timestamp_ns, sizeof(uint64_t));

    struct iovec iov[1];
    iov[0].iov_base = slot.data;
    iov[0].iov_len  = sizeof(slot.data);
    if (!tier0_append(iov, 1, timestamp_ns)) {
        return; // Tier-0 failure: do not publish control event downstream.
    }

    if (market_events_) {
        market_events_->push(slot);
    }
}

bool FeedIngestor::tier0_flush_if_due(uint64_t now_ns) noexcept {
    // Explicit disabled mode for development/unit tests.
    if (tier0_fd_ < 0) {
        return true;
    }

    if (tier0_pending_bytes_ == 0) {
        return true;
    }

    const bool bytes_due =
        tier0_pending_bytes_ >= kTier0FlushBytes;

    const bool time_due =
        tier0_last_flush_ns_ != 0 &&
        now_ns >= tier0_last_flush_ns_ &&
        (now_ns - tier0_last_flush_ns_) >= kTier0FlushIntervalNs;

    if (!bytes_due && !time_due) {
        return true;
    }

    const uint64_t fsync_start_ns = []() noexcept -> uint64_t {
#if defined(__linux__)
        struct timespec ts{};
        ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
             + static_cast<uint64_t>(ts.tv_nsec);
#else
        return 0;
#endif
    }();

    const int fsync_rc = ::fsync(tier0_fd_);

    const uint64_t fsync_end_ns = []() noexcept -> uint64_t {
#if defined(__linux__)
        struct timespec ts{};
        ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
             + static_cast<uint64_t>(ts.tv_nsec);
#else
        return 0;
#endif
    }();

#if defined(__linux__)
    if (fsync_end_ns >= fsync_start_ns &&
        (fsync_end_ns - fsync_start_ns) >= 1'000'000ULL) {
        std::fprintf(
            stderr,
            "[TIER0] slow fsync=%llu us pending=%zu now=%llu\\n",
            static_cast<unsigned long long>(
                (fsync_end_ns - fsync_start_ns) / 1'000ULL),
            tier0_pending_bytes_,
            static_cast<unsigned long long>(now_ns));
    }
#endif

    if (fsync_rc != 0) {
        // Sticky fault: a storage failure is non-recoverable within a run.
        // Set tier0_fault_ so process_payload() cannot clear FEED_STALE.
        tier0_fault_ = true;
        return false;
    }

    tier0_pending_bytes_ = 0;
    tier0_last_flush_ns_ = now_ns;
    return true;
}

bool FeedIngestor::tier0_append(
    const struct iovec* iov,
    int iovcnt,
    uint64_t now_ns) noexcept
{
    // Sticky fault check: Tier-0 failures are non-recoverable within a run.
    // MUST prevent any downstream publication after a durability fault to 
    // satisfy I-S2-001 (Tier-0 Ordering Invariant).
    if (tier0_fault_) {
        return false;
    }

    // Negative FD is the explicit test/development "Tier-0 disabled" mode.
    if (tier0_fd_ < 0) {
        return true;
    }

    if (iov == nullptr || iovcnt <= 0) {
        return false;
    }

    std::size_t expected = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].iov_base == nullptr) {
            return false;
        }
        expected += iov[i].iov_len;
    }

#if defined(_WIN32)
    ssize_t written = writev(tier0_fd_, iov, iovcnt);
#else
    ssize_t written = ::writev(tier0_fd_, iov, iovcnt);
#endif

    if (written < 0 ||
        static_cast<std::size_t>(written) != expected) {
        return false;
    }

    tier0_pending_bytes_ += expected;

    if (tier0_last_flush_ns_ == 0) {
        tier0_last_flush_ns_ = now_ns;
    }

    // AD-003: flush at 64KB or 5ms, whichever first.
    // The flush occurs before SPSC publication of this record.
    return tier0_flush_if_due(now_ns);
}


} // namespace egbos::l0
