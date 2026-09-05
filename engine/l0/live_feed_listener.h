// engine/l0/live_feed_listener.h
// EGBOS ENGINE_V3 — L0 Live Feed Listener
//
// Specification  : S5-REQ-001 (Live Feed Integration), S5-REQ-005 (Zero Order Path)
// Constraints    : Zero dynamic allocation (GC-E-010).
//                  Strictly read-only network layer. No outbound paths exist.

#pragma once

#include "ingestor.h"
#include "feed_source.h"
#include <cstdint>
#include <string>
#include <atomic>

#if defined(__linux__)
#include <linux/io_uring.h>
#include <sys/uio.h>
#endif

namespace egbos::l0 {

class LiveFeedListener {
public:
    // Configures the UDP listener and binds it to the specified multicast group / port.
    // Throws std::runtime_error on failure (startup phase only).
    LiveFeedListener(FeedIngestor& ingestor, const std::string& multicast_ip, uint16_t port);

    ~LiveFeedListener();

    // Hot-path polling loop. Returns false if interrupted.
    // Reads UDP datagrams and passes them directly to FeedIngestor::process_payload.
    // Handles staleness checks via FeedIngestor::check_staleness when idle.
    void run_loop() noexcept;

    // Signal safe method to gracefully terminate the loop.
    void request_stop() noexcept;

    ListenerState state() const noexcept { return state_.load(std::memory_order_relaxed); }
    FeedProvenance provenance() const noexcept { return FeedProvenance::EXTERNAL_LIVE; }

private:
    FeedIngestor& ingestor_;
    int fd_{-1};
    std::atomic<bool> running_{false};
    std::atomic<ListenerState> state_{ListenerState::INITIALIZING};

#if defined(__linux__)
    // Minimal io_uring state (D6)
    int uring_fd_{-1};
    void* sq_ring_ptr_{nullptr};
    std::size_t sq_ring_sz_{0};
    unsigned* sq_head_{nullptr};
    unsigned* sq_tail_{nullptr};
    unsigned* sq_mask_{nullptr};
    unsigned* sq_array_{nullptr};

    void* cq_ring_ptr_{nullptr};
    std::size_t cq_ring_sz_{0};
    unsigned* cq_head_{nullptr};
    unsigned* cq_tail_{nullptr};
    unsigned* cq_mask_{nullptr};
    struct io_uring_cqe* cqes_{nullptr};

    struct io_uring_sqe* sqes_{nullptr};
    void* sqes_ptr_{nullptr};
    std::size_t sqes_sz_{0};

    bool single_mmap_{false};
    bool recv_in_flight_{false}; // D6: one RECV SQE in-flight at a time; prevents SQ overflow

    // IORING_OP_RECV buffer
    struct iovec registered_iov_{};
    uint8_t buffer_[2048]{}; // D8: 2048 placeholder
#else
    uint8_t buffer_[2048]{}; // D8: 2048 placeholder
#endif
};

} // namespace egbos::l0
