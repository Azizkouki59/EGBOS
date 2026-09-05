// engine/l0/live_feed_listener.cpp
// EGBOS ENGINE_V3 — L0 Live Feed Listener Implementation
//
// Specification  : S5-REQ-001 (Live Feed Integration), S5-REQ-005 (Zero Order Path)
// Constraints    : Zero dynamic allocation (GC-E-010).

#include "live_feed_listener.h"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <cerrno>

#if defined(_WIN32)
// DEVELOPMENT/TEST ONLY — NOT TIER-0
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <algorithm>

#ifndef __NR_io_uring_setup
#define __NR_io_uring_setup 425
#endif
#ifndef __NR_io_uring_enter
#define __NR_io_uring_enter 426
#endif
#endif

namespace egbos::l0 {

#if defined(__linux__)
namespace {
inline uint64_t monotonic_raw_ns() noexcept {
    struct timespec ts;
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
}
}
#endif

LiveFeedListener::LiveFeedListener(FeedIngestor& ingestor, const std::string& multicast_ip, uint16_t port)
    : ingestor_(ingestor) {

#if defined(_WIN32)
    // DEVELOPMENT/TEST ONLY — NOT TIER-0
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif

    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    // Set non-blocking
#if defined(_WIN32)
    // DEVELOPMENT/TEST ONLY — NOT TIER-0
    u_long mode = 1;
    ioctlsocket(fd_, FIONBIO, &mode);
#else
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
#endif

    // Allow multiple sockets to use the same PORT number
    int reuse = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        throw std::runtime_error("Setting SO_REUSEADDR error");
    }

    struct sockaddr_in local_addr;
    std::memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd_, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        throw std::runtime_error("Failed to bind UDP socket");
    }

    // Join the multicast group
    struct ip_mreq group;
    group.imr_multiaddr.s_addr = inet_addr(multicast_ip.c_str());
    group.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&group, sizeof(group)) < 0) {
        state_.store(ListenerState::NON_OPERATIONAL_MULTICAST_FAILED, std::memory_order_relaxed);
    } else {
        state_.store(ListenerState::OPERATIONAL, std::memory_order_relaxed);
    }

#if defined(__linux__)
    struct io_uring_params params {};
    uring_fd_ = static_cast<int>(::syscall(__NR_io_uring_setup, 4, &params));
    if (uring_fd_ < 0) {
        throw std::runtime_error("io_uring_setup failed");
    }

    sq_ring_sz_ = params.sq_off.array + params.sq_entries * sizeof(unsigned);
    cq_ring_sz_ = params.cq_off.cqes + params.cq_entries * sizeof(struct io_uring_cqe);
    sqes_sz_ = params.sq_entries * sizeof(struct io_uring_sqe);

    single_mmap_ = (params.features & (1U << 0)) != 0;
    if (single_mmap_) {
        sq_ring_sz_ = std::max(sq_ring_sz_, cq_ring_sz_);
        cq_ring_sz_ = sq_ring_sz_;
    }

    sq_ring_ptr_ = ::mmap(nullptr, sq_ring_sz_, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, uring_fd_, IORING_OFF_SQ_RING);
    if (single_mmap_) {
        cq_ring_ptr_ = sq_ring_ptr_;
    } else {
        cq_ring_ptr_ = ::mmap(nullptr, cq_ring_sz_, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, uring_fd_, IORING_OFF_CQ_RING);
    }
    sqes_ptr_ = ::mmap(nullptr, sqes_sz_, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, uring_fd_, IORING_OFF_SQES);

    auto* sq = static_cast<uint8_t*>(sq_ring_ptr_);
    sq_head_ = reinterpret_cast<unsigned*>(sq + params.sq_off.head);
    sq_tail_ = reinterpret_cast<unsigned*>(sq + params.sq_off.tail);
    sq_mask_ = reinterpret_cast<unsigned*>(sq + params.sq_off.ring_mask);
    sq_array_ = reinterpret_cast<unsigned*>(sq + params.sq_off.array);

    auto* cq = static_cast<uint8_t*>(cq_ring_ptr_);
    cq_head_ = reinterpret_cast<unsigned*>(cq + params.cq_off.head);
    cq_tail_ = reinterpret_cast<unsigned*>(cq + params.cq_off.tail);
    cq_mask_ = reinterpret_cast<unsigned*>(cq + params.cq_off.ring_mask);
    cqes_ = reinterpret_cast<struct io_uring_cqe*>(cq + params.cq_off.cqes);
    sqes_ = static_cast<struct io_uring_sqe*>(sqes_ptr_);
#endif
}

LiveFeedListener::~LiveFeedListener() {
#if defined(__linux__)
    if (sqes_ptr_) ::munmap(sqes_ptr_, sqes_sz_);
    if (!single_mmap_ && cq_ring_ptr_) ::munmap(cq_ring_ptr_, cq_ring_sz_);
    if (sq_ring_ptr_) ::munmap(sq_ring_ptr_, sq_ring_sz_);
    if (uring_fd_ >= 0) ::close(uring_fd_);
#endif
    if (fd_ >= 0) {
#if defined(_WIN32)
        closesocket(fd_);
        WSACleanup();
#else
        close(fd_);
#endif
    }
}

void LiveFeedListener::run_loop() noexcept {
    if (state_.load(std::memory_order_relaxed) == ListenerState::NON_OPERATIONAL_MULTICAST_FAILED) {
        return; // D1: Refuse to enter hot path if non-operational
    }

    running_.store(true, std::memory_order_relaxed);

    while (running_.load(std::memory_order_relaxed)) {
#if defined(_WIN32)
        // DEVELOPMENT/TEST ONLY — NOT TIER-0
        LARGE_INTEGER freq, cnt;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&cnt);
        uint64_t current_time_ns = static_cast<uint64_t>(cnt.QuadPart * 1000000000ULL / freq.QuadPart);

        int nbytes = recv(fd_, reinterpret_cast<char*>(buffer_), sizeof(buffer_), 0);
        if (nbytes > 0) {
            ingestor_.process_payload(buffer_, static_cast<std::size_t>(nbytes), current_time_ns);
        } else if (nbytes == 0) {
            continue;
        } else {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                ingestor_.check_staleness(current_time_ns);
            } else if (err == WSAEINTR) {
                continue;
            } else {
                state_.store(ListenerState::FAULTED, std::memory_order_relaxed);
                break;
            }
        }
#else
        // Linux Tier-0 io_uring path (D6)
        //
        // D6 SQE lifecycle invariant:
        //   Only one IORING_OP_RECV is submitted at a time.
        //   recv_in_flight_ is set on submission, cleared on CQE drain.
        //   When in-flight, poll CQ only (to_submit=0) to avoid SQ overflow.
        //   When not in-flight, submit the next RECV SQE (to_submit=1).
        //   Both paths use min_complete=0 for D2 termination compatibility.

        unsigned to_submit = 0U;

        if (!recv_in_flight_) {
            // Submit one IORING_OP_RECV into the SQ.
            unsigned tail = __atomic_load_n(sq_tail_, __ATOMIC_RELAXED);
            unsigned idx = tail & *sq_mask_;

            struct io_uring_sqe* sqe = &sqes_[idx];
            std::memset(sqe, 0, sizeof(*sqe));
            sqe->opcode    = IORING_OP_RECV;
            sqe->fd        = fd_;
            sqe->addr      = reinterpret_cast<uint64_t>(buffer_);
            sqe->len       = sizeof(buffer_);
            sqe->user_data = 1;

            sq_array_[idx] = idx;
            __atomic_store_n(sq_tail_, tail + 1, __ATOMIC_RELEASE);
            to_submit = 1U;
            recv_in_flight_ = true;
        }

        // Enter io_uring: submit if to_submit=1, poll CQ if to_submit=0.
        // min_complete=1 — Wait for exactly 1 CQE (the outstanding RECV or its shutdown).
        // Preserves D2 atomic termination because request_stop() calls shutdown(),
        // which immediately wakes this syscall with a CQE.
        long ret = ::syscall(__NR_io_uring_enter, uring_fd_, to_submit, 1U,
                             IORING_ENTER_GETEVENTS, nullptr,
                             static_cast<std::size_t>(0));

        uint64_t current_time_ns = monotonic_raw_ns(); // D4: CLOCK_MONOTONIC_RAW

        if (ret < 0) {
            if (errno == EINTR) continue;
            state_.store(ListenerState::FAULTED, std::memory_order_relaxed);
            break;
        }

        // Drain at most one CQE per iteration.
        unsigned ch = __atomic_load_n(cq_head_, __ATOMIC_ACQUIRE);
        unsigned ct = __atomic_load_n(cq_tail_, __ATOMIC_ACQUIRE);
        if (ch != ct) {
            unsigned ci = ch & *cq_mask_;
            int32_t res = cqes_[ci].res;
            __atomic_store_n(cq_head_, ch + 1, __ATOMIC_RELEASE);
            recv_in_flight_ = false; // CQE consumed — next iteration may submit.

            if (res > 0) {
                ingestor_.process_payload(buffer_, static_cast<std::size_t>(res), current_time_ns);
            } else if (res == -EAGAIN || res == -EWOULDBLOCK || res == 0) {
                // res == 0 (EOF/shutdown) naturally falls here or falls through safely.
                // It ensures staleness check runs if we unblocked, but running_ guard handles termination.
                ingestor_.check_staleness(current_time_ns);
            } else if (res == -EINTR) {
                continue;
            } else if (res < 0) {
                // Shutdown could produce negative error (e.g. -ECANCELED, -EBADF).
                // If running_ is false, it's a graceful stop.
                if (!running_.load(std::memory_order_relaxed)) {
                    break;
                }
                state_.store(ListenerState::FAULTED, std::memory_order_relaxed);
                break;
            }
        }
#endif
    }
}

void LiveFeedListener::request_stop() noexcept {
    running_.store(false, std::memory_order_relaxed);
#if defined(_WIN32)
    if (fd_ >= 0) shutdown(fd_, SD_BOTH);
#else
    if (fd_ >= 0) ::shutdown(fd_, SHUT_RDWR);
#endif
}

} // namespace egbos::l0
