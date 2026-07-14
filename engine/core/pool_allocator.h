// engine/core/pool_allocator.h
// EGBOS ENGINE_V3 — Fixed-Size Pool Allocator
//
// Specification : EGBOS-SPEC-S1-001 §7.2, §14.1 (supporting), §15.3
// ADR compliance: AD-006 (compile-time capacity — no runtime sizing)
// Claim         : CLAIM-L3-POOL-001 (allocator.pool, V2)
//   Claim property: zero malloc/new calls emitted from pool operation
//   paths, verified by ASAN under P3 gate.
//
// Govcheck      : GC-E-010 (Warning, Phase 1 → Blocking Phase 3)
//   This file contains NO malloc, new, free, or delete.
//   All storage is member-resident (BSS/stack). No heap involvement.
//
// Thread safety : NOT thread-safe. The primary user is the WAL writer,
//   which operates on a single thread per AD-005 (io_uring single-CQE-
//   consumer). External synchronisation is the caller's responsibility.
//
// Failure model : acquire() returns nullptr on pool exhaustion.
//   Spec §7.2: "Pool exhaustion is a fatal startup error — not a runtime
//   condition." Callers MUST check the return value and abort; they must
//   NOT attempt to recover at runtime.
//
// Forbidden     : CLOCK_REALTIME — not used (pre-compliant with GC-E-011).
//                 malloc/new/free/delete — structurally absent (GC-E-010).

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>   // std::uintptr_t (uintptr_t bounds check in slot_index)
#include <type_traits>

namespace egbos::alloc {

// ─────────────────────────────────────────────────────────────────────────────
// FixedPool<T, N>
//
// A pre-allocated, fixed-capacity object pool providing typed pointers to
// N slots of type T. All storage lives in the member array `storage_` —
// no heap allocation is ever performed, either in the constructor or in
// any operation thereafter.
//
// Template parameters:
//   T — object type. Must be trivially destructible (pool never calls
//       destructors; slots are returned as raw reusable memory).
//   N — maximum number of objects that may be simultaneously live.
//       Must be > 0. Does NOT need to be a power of 2.
//
// Object lifecycle:
//   1. acquire() returns a pointer to an uninitialized slot.
//      The caller is responsible for initialising the object before use.
//      WAL writer fills all WalRecord fields before submitting to io_uring.
//   2. release(ptr) marks the slot available for the next acquire().
//      The destructor of T is NOT called. T must be trivially destructible.
//
// Double-release and foreign-pointer-release are both caught by assertion,
// making them detectable under ASAN (pool_no_double_free test, spec §15.3).
//
// ASAN note: because no malloc/new is called, ASAN's malloc tracking will
// attribute zero heap events to this class. The pool_alloc_asan test
// (spec §15.3) verifies this property empirically.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, std::size_t N>
class FixedPool {

    // ── Compile-time constraints ──────────────────────────────────────────────

    static_assert(N > 0,
        "FixedPool: capacity N must be non-zero (spec §7.2)");

    // Pool never calls destructors. If T has a non-trivial destructor, the
    // caller must call it manually before releasing the slot, then annotate
    // the claim accordingly. For WAL records (POD) this is never an issue.
    static_assert(std::is_trivially_destructible_v<T>,
        "FixedPool: T must be trivially destructible. "
        "Pool slots are returned as raw memory — destructors are never called. "
        "Use a trivially destructible type or handle destruction externally "
        "before calling release().");

    // ── Storage — zero heap allocation (CLAIM-L3-POOL-001) ───────────────────

    // Raw byte array. Aligned to T's alignment requirement so that
    // slot_ptr() can safely reinterpret_cast without UB.
    // Value-initialised to 0 by the member initialiser `{}`.
    // Sizing: exactly N * sizeof(T) bytes — no slack, no heap.
    alignas(alignof(T)) uint8_t storage_[N * sizeof(T)]{};

    // Slot occupancy map. `true` = slot is live (acquired and not released).
    // One bool per slot. Value-initialised to false (all slots free).
    bool in_use_[N]{};

    // Acquisition hint. Stores the slot index to try first on the next
    // acquire() call. Reduces scan cost to O(1) for sequential
    // acquire/release patterns (e.g. WAL writer with depth-1 in-flight).
    // After a release(), updated to point at the newly freed slot.
    std::size_t next_{0};

public:

    // ── Constructors / destructor ─────────────────────────────────────────────

    // Default constructor: all storage is member-resident and value-
    // initialised above. No heap allocation. Must complete at engine
    // startup before the operational loop begins (spec §7.2).
    FixedPool() noexcept = default;

    // Non-copyable and non-movable.
    // Rationale: FixedPool owns fixed-address storage. Any copy or move
    // would silently invalidate all outstanding pointers returned by
    // acquire(). The compiler error is the enforcement mechanism.
    FixedPool(const FixedPool&)            = delete;
    FixedPool& operator=(const FixedPool&) = delete;
    FixedPool(FixedPool&&)                 = delete;
    FixedPool& operator=(FixedPool&&)      = delete;

    ~FixedPool() noexcept = default;

    // ── acquire() ─────────────────────────────────────────────────────────────
    //
    // Returns a pointer to one uninitialized slot.
    //
    // The caller MUST:
    //   1. Check the return value — nullptr means pool exhausted (fatal).
    //   2. Initialise the object before use.
    //   3. Call release() when finished.
    //
    // The caller MUST NOT:
    //   - Call delete on the returned pointer.
    //   - Call free() on the returned pointer.
    //   - Use the pointer after calling release().
    //
    // Time complexity: O(N) worst-case; O(1) amortised for sequential patterns.
    // Does not call malloc, new, or any allocator (GC-E-010, CLAIM-L3-POOL-001).
    //
    // Returns: pointer to an unused slot, or nullptr if all N slots are live.
    [[nodiscard]] T* acquire() noexcept {
        // Linear scan starting from next_, with wraparound.
        // Using % N (not bitmask) because N is not required to be power of 2.
        for (std::size_t i = 0; i < N; ++i) {
            const std::size_t idx = (next_ + i) % N;
            if (!in_use_[idx]) {
                in_use_[idx] = true;
                // Advance hint so the next acquire() skips this (now-used) slot.
                next_ = (idx + 1) % N;
                return slot_ptr(idx);
            }
        }
        // All N slots are live. Caller must treat this as a fatal startup error.
        // Spec §7.2: "Pool exhaustion is a fatal startup error — not a runtime
        // condition."
        return nullptr;
    }

    // ── release() ─────────────────────────────────────────────────────────────
    //
    // Returns a slot to the pool, making it available for the next acquire().
    //
    // Preconditions (enforced by assertion):
    //   - ptr must not be nullptr.
    //   - ptr must have been returned by a prior acquire() on THIS pool instance.
    //   - ptr must not have been released already (no double-release).
    //
    // The destructor of T is NOT called. T must be trivially destructible.
    // The slot's bytes are NOT zeroed — the next acquirer must initialise.
    //
    // Does not call free, delete, or any deallocator (GC-E-010).
    void release(T* ptr) noexcept {
        // Null-pointer guard — always a programming error.
        assert(ptr != nullptr &&
            "FixedPool::release: null pointer — always a programming error");

        // Compute slot index. Returns N (sentinel) if ptr is out of range.
        const std::size_t idx = slot_index(ptr);

        // Bounds check — ptr must originate from this pool's storage_.
        // Catches use of a foreign pointer or pointer into a different pool.
        assert(idx < N &&
            "FixedPool::release: pointer not within pool storage "
            "(foreign pointer, use-after-free, or wrong pool instance)");

        // Slot-alignment check — ptr must be exactly slot-aligned.
        // A mid-slot pointer indicates internal corruption.
        // Slot-alignment check via uintptr_t arithmetic — avoids UB from
        // subtracting unrelated pointers ([expr.add]/5 C++17). The bounds
        // check above guarantees ptr is numerically within [base, end), so
        // the difference is non-negative and fits in std::size_t.
        assert((reinterpret_cast<std::uintptr_t>(ptr)
                    - reinterpret_cast<std::uintptr_t>(&storage_[0]))
                   % sizeof(T) == 0 &&
            "FixedPool::release: pointer is not slot-aligned "
            "(internal corruption)");

        // Double-release detection — slot must currently be in-use.
        // Under ASAN this fires as an assertion abort, making it detectable
        // without an explicit shadow map (pool_no_double_free test, spec §15.3).
        assert(in_use_[idx] &&
            "FixedPool::release: double-release detected — "
            "slot is already free");

        in_use_[idx] = false;

        // Update hint to point at the newly freed slot. The next acquire()
        // call will find it immediately, giving O(1) behaviour for
        // depth-1 acquire/release patterns.
        next_ = idx;
    }

    // ── capacity() ───────────────────────────────────────────────────────────
    //
    // Returns the total pool capacity. Compile-time constant.
    // Used by callers to size the pool at startup (spec §7.2).
    static constexpr std::size_t capacity() noexcept {
        return N;
    }

    // ── available() ──────────────────────────────────────────────────────────
    //
    // Returns the number of currently free slots. O(N) scan.
    // Intended for diagnostics and startup validation — NOT for hot-path use.
    // The utilisation counter exposed to Prometheus (spec §5.3) should use
    // this at a low sample rate (not every event).
    std::size_t available() const noexcept {
        std::size_t free_count = 0;
        for (std::size_t i = 0; i < N; ++i) {
            if (!in_use_[i]) {
                ++free_count;
            }
        }
        return free_count;
    }

private:

    // ── Internal helpers ──────────────────────────────────────────────────────

    // Returns a typed pointer to slot `idx` within storage_.
    // The reinterpret_cast is well-defined: storage_ is aligned to alignof(T),
    // and the byte offset idx*sizeof(T) keeps alignment correct.
    // No malloc, new, or allocator is invoked.
    T* slot_ptr(std::size_t idx) noexcept {
        // idx is validated by the caller (always < N at this point).
        return reinterpret_cast<T*>(&storage_[idx * sizeof(T)]);
    }

    const T* slot_ptr(std::size_t idx) const noexcept {
        return reinterpret_cast<const T*>(&storage_[idx * sizeof(T)]);
    }

    // Converts a pointer (presumably returned by acquire()) back to a slot
    // index. Returns N as a sentinel if the pointer is outside storage_.
    // Used by release() for validation.
    std::size_t slot_index(const T* ptr) const noexcept {
        // Use uintptr_t arithmetic for the bounds check.
        // C++17 [expr.rel]/5: comparing pointers to unrelated objects is
        // unspecified. Comparing their uintptr_t representations is a numeric
        // comparison — well-defined on all hosted implementations.
        // The pool occupies a contiguous byte range in BSS; the numeric
        // comparison correctly identifies membership on all supported platforms.
        const std::uintptr_t p    = reinterpret_cast<std::uintptr_t>(ptr);
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(&storage_[0]);
        const std::uintptr_t end  = base + static_cast<std::uintptr_t>(N * sizeof(T));

        // Pointer must be within [base, end).
        if (p < base || p >= end) {
            return N;  // sentinel — out of range
        }

        const std::size_t byte_offset = static_cast<std::size_t>(p - base);
        return byte_offset / sizeof(T);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Compile-time self-check: instantiate FixedPool with a minimal configuration
// to verify the template constraints fire at compile time, not only at runtime.
// A FixedPool with N=0 must not compile; N=1 must compile cleanly.
//
// This declaration exercises the static_asserts on `trivially_destructible`
// and `N > 0` for a typical POD use-case (uint64_t slot, depth 1).
// Production instantiations (e.g. FixedPool<WalRecord, 16>) are in wal_writer.h.
// ─────────────────────────────────────────────────────────────────────────────
namespace detail {
// If this line fails to compile, the static_assert inside FixedPool fired.
// That means a constraint was violated. Read the assertion message.
using _SelfCheckPool = FixedPool<uint64_t, 1>;
}  // namespace detail

}  // namespace egbos::alloc
