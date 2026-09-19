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

    // Transition back to FEED_NORMAL if we were in another state.
    if (current_state_ != FeedState::FEED_NORMAL) [[unlikely]] {
        current_state_ = FeedState::FEED_NORMAL;
        // S1 SPEC CORRECTION: L0 is execution plane. It cannot write to gov_flags_.
        // Governance subsystem monitors Tier-0 and sets gov_flags_ out-of-band.
    }

    // Forward the complete, unstripped payload.
    normalize_and_forward(payload, length, rx_timestamp_ns);
}

void FeedIngestor::check_staleness(uint64_t current_time_ns) noexcept {
    if (last_recv_time_ns_ == 0) [[unlikely]] return;

    if (current_state_ != FeedState::FEED_STALE) [[likely]] {
        if (current_time_ns - last_recv_time_ns_ > kStaleThresholdNs) [[unlikely]] {
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
        ingestor->tier0_append(iov, 1);

        // Hand over to L1 via the SPSC ring.
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
    tier0_append(iov, 1);

    if (market_events_) {
        market_events_->push(slot);
    }
}

void FeedIngestor::tier0_append(const struct iovec* iov, int iovcnt) noexcept {
    // writev guarantees atomic append across the gathered buffers.
    // O_APPEND must be set on tier0_fd_ during initialization.
#if defined(_WIN32)
    ssize_t written = writev(tier0_fd_, iov, iovcnt);
#else
    ssize_t written = ::writev(tier0_fd_, iov, iovcnt);
#endif
    (void)written;
    
    // NOTE: True HFT threshold fsync would batch here based on byte count or time
    // to avoid blocking the hot-path network thread on NVMe flushes.
}

} // namespace egbos::l0
