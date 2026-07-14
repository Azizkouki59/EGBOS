// engine/core/spsc_ring.h
// EGBOS ENGINE_V3 — Lock-free SPSC Ring Buffer
//
// Specification  : EGBOS-SPEC-S1-001 §5.1–§5.4, §13.2, §14.2
// ADR compliance : AD-006 (power of two capacity)
// Claims         : CLAIM-SPSC-001
// Govcheck       : GC-E-010 (no malloc/new)

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace egbos::core {

// N sourced from timing_constants.json (S1 timing_constants.json is a schema stub).
// We use a constexpr default for the spin yield threshold.
inline constexpr std::size_t kSpscSpinYieldThreshold = 1000;

template <typename T, std::size_t Capacity>
class SpscRing {
    // ── Compile-time Constraints (Spec §5.2, §14.2) ──────────────────────────
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
        "SpscRing capacity must be a power of 2 (AD-006)");
    static_assert(std::is_trivially_copyable_v<T>,
        "SpscRing slot type must be trivially copyable (§14.2)");
    static_assert(std::is_default_constructible_v<T>,
        "SpscRing slot type must be default constructible");

    static constexpr std::size_t kMask = Capacity - 1;
    static constexpr std::size_t kCacheLineSize = 64;

public:
    SpscRing() noexcept = default;
    ~SpscRing() noexcept = default;

    // Zero heap allocation (GC-E-010) — prevent copying and moving
    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;
    SpscRing(SpscRing&&) = delete;
    SpscRing& operator=(SpscRing&&) = delete;

    // ── Producer Interface (Spec §5.2, §5.3, §13.2, §14.2) ───────────────────
    // push() — called from producer thread only; spins on full
    void push(const T& item) noexcept {
        assert_producer_thread();

        // Producer reads its own index (relaxed)
        const uint64_t head = head_.load(std::memory_order_relaxed);
        
        // Check if full (using cached tail to avoid atomic read if possible)
        if (head - tail_cache_ == Capacity) {
            std::size_t spins = 0;
            while (true) {
                // Producer reads consumer index with acquire
                tail_cache_ = tail_.load(std::memory_order_acquire);
                if (head - tail_cache_ < Capacity) {
                    break; // Space available
                }
                
                // Spin yield backpressure (§5.3)
                if (++spins >= kSpscSpinYieldThreshold) {
#if defined(__linux__)
                    ::sched_yield();
#endif
                } else {
#if defined(__x86_64__) || defined(_M_X64)
                    _mm_pause();
#endif
                }
            }
        }

        // Write to slot
        slots_[head & kMask] = item;

        // Producer index: seq_cst on write per spec §5.2
        head_.store(head + 1, std::memory_order_seq_cst);
    }

    // ── Consumer Interface (Spec §5.2, §13.2, §14.2) ─────────────────────────
    // pop() — called from consumer thread only; returns false if empty
    [[nodiscard]] bool pop(T& item) noexcept {
        assert_consumer_thread();

        // Consumer reads its own index: relaxed on read per spec §5.2
        const uint64_t tail = tail_.load(std::memory_order_relaxed);

        // Consumer reads producer index: acquire to see slot writes
        if (head_cache_ == tail) {
            head_cache_ = head_.load(std::memory_order_acquire);
            if (head_cache_ == tail) {
                return false; // Empty
            }
        }

        // Read slot
        item = slots_[tail & kMask];
        
        // Update consumer index: release to ensure slot read completes before
        // producer can overwrite it.
        tail_.store(tail + 1, std::memory_order_release);
        
        return true;
    }

private:
    // Thread identity assertion helpers (§5.4)
    void assert_producer_thread() noexcept {
#if defined(__linux__) && !defined(NDEBUG)
        const pthread_t id = ::pthread_self();
        pthread_t expected = 0;
        if (producer_id_.compare_exchange_strong(expected, id, std::memory_order_relaxed)) {
            // First call registers the thread
        } else {
            assert(pthread_equal(expected, id) && "SPSC-SINGLE violation: multiple producers detected");
        }
#endif
    }

    void assert_consumer_thread() noexcept {
#if defined(__linux__) && !defined(NDEBUG)
        const pthread_t id = ::pthread_self();
        pthread_t expected = 0;
        if (consumer_id_.compare_exchange_strong(expected, id, std::memory_order_relaxed)) {
            // First call registers the thread
        } else {
            assert(pthread_equal(expected, id) && "SPSC-SINGLE violation: multiple consumers detected");
        }
#endif
    }

    // ── Cache-line isolated members (§5.2, §13.2) ────────────────────────────
    
    // Producer cache line
    alignas(kCacheLineSize) std::atomic<uint64_t> head_{0}; // Producer index
    uint64_t tail_cache_{0};                                // Owned by producer
#if defined(__linux__) && !defined(NDEBUG)
    std::atomic<pthread_t> producer_id_{0};
#endif

    // Consumer cache line
    alignas(kCacheLineSize) std::atomic<uint64_t> tail_{0}; // Consumer index
    uint64_t head_cache_{0};                                // Owned by consumer
#if defined(__linux__) && !defined(NDEBUG)
    std::atomic<pthread_t> consumer_id_{0};
#endif

    // Slots (must be separate from consumer index to prevent false sharing)
    alignas(kCacheLineSize) T slots_[Capacity]{};
};

} // namespace egbos::core
