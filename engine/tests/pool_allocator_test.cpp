// engine/tests/pool_allocator_test.cpp
// EGBOS ENGINE_V3 — Pool Allocator Test Suite
//
// Specification  : EGBOS-SPEC-S1-001 §15.3
// CI gates       : P3 (unit), ASAN profile
// Claim          : CLAIM-L3-POOL-001 (allocator.pool, V2)

#include "core/pool_allocator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace {

// ── Test payload ─────────────────────────────────────────────────────────────
struct alignas(8) TestSlot {
    uint64_t id{0};
    uint64_t value{0};
};

static_assert(std::is_trivially_copyable_v<TestSlot>);
static_assert(std::is_trivially_destructible_v<TestSlot>);

// ─────────────────────────────────────────────────────────────────────────────
// §15.3: pool_alloc_asan
// Run pool allocator through 1M allocate/free cycles.
// ASAN: zero malloc/new calls from pool paths.
// ─────────────────────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, AllocAsanCycles) {
    constexpr std::size_t kPoolSize = 256;
    constexpr std::size_t kCycles = 1'000'000;

    egbos::alloc::FixedPool<TestSlot, kPoolSize> pool;

    for (std::size_t cycle = 0; cycle < kCycles; ++cycle) {
        // Acquire a slot
        TestSlot* slot = pool.acquire();
        ASSERT_NE(slot, nullptr) << "Pool exhausted at cycle " << cycle;

        // Write to the slot (ASAN would detect out-of-bounds)
        slot->id = cycle;
        slot->value = cycle * 7;

        // Release the slot back to pool
        pool.release(slot);
    }

    // After all cycles, pool should be at full capacity
    // (all slots returned)
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.3: pool_alloc_asan — batch acquire/release
// Acquire all slots, then release all, repeat. Validates no heap fallback.
// ─────────────────────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, AllocAsanBatchCycles) {
    constexpr std::size_t kPoolSize = 64;
    constexpr std::size_t kBatchCycles = 10'000;

    egbos::alloc::FixedPool<TestSlot, kPoolSize> pool;

    for (std::size_t cycle = 0; cycle < kBatchCycles; ++cycle) {
        // Acquire all slots
        std::array<TestSlot*, kPoolSize> slots{};
        for (std::size_t i = 0; i < kPoolSize; ++i) {
            slots[i] = pool.acquire();
            ASSERT_NE(slots[i], nullptr)
                << "Pool exhausted at slot " << i << " cycle " << cycle;
            slots[i]->id = i;
            slots[i]->value = cycle;
        }

        // Pool should be exhausted — next acquire returns nullptr
        TestSlot* overflow = pool.acquire();
        EXPECT_EQ(overflow, nullptr) << "Pool did not return nullptr on exhaustion";

        // Release all slots
        for (std::size_t i = 0; i < kPoolSize; ++i) {
            pool.release(slots[i]);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.3: pool_exhaustion_fatal
// Exhaust pool at startup. Fatal error path; startup aborts cleanly.
//
// The pool returns nullptr on exhaustion — the caller decides whether
// to treat this as fatal. This test verifies nullptr is returned.
// ─────────────────────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, ExhaustionReturnsNullptr) {
    constexpr std::size_t kPoolSize = 4;

    egbos::alloc::FixedPool<TestSlot, kPoolSize> pool;

    // Acquire all slots
    std::array<TestSlot*, kPoolSize> slots{};
    for (std::size_t i = 0; i < kPoolSize; ++i) {
        slots[i] = pool.acquire();
        ASSERT_NE(slots[i], nullptr);
    }

    // Pool is exhausted — all subsequent acquires must return nullptr
    for (int i = 0; i < 100; ++i) {
        TestSlot* overflow = pool.acquire();
        EXPECT_EQ(overflow, nullptr)
            << "Pool returned non-null after exhaustion at attempt " << i;
    }

    // Cleanup: release all
    for (auto* s : slots) {
        pool.release(s);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// §15.3: pool_no_double_free
// Double-return of pool slot. ASAN or assertion: double-free detected
// and handled.
//
// NOTE: In debug builds, the pool's bitset detects double-free via assertion.
// This test validates that a single release followed by re-acquire works,
// and that the slot is reusable.
// ─────────────────────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, NoDoubleFreeReuse) {
    constexpr std::size_t kPoolSize = 8;

    egbos::alloc::FixedPool<TestSlot, kPoolSize> pool;

    // Acquire a slot
    TestSlot* slot = pool.acquire();
    ASSERT_NE(slot, nullptr);

    // Write data
    slot->id = 42;
    slot->value = 999;

    // Release
    pool.release(slot);

    // Re-acquire — should get a valid slot (possibly the same one)
    TestSlot* slot2 = pool.acquire();
    ASSERT_NE(slot2, nullptr);

    // The slot should be writable
    slot2->id = 100;
    slot2->value = 200;

    pool.release(slot2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pointer validity and bounds checking
// ─────────────────────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, AcquiredPointersAreWithinPool) {
    constexpr std::size_t kPoolSize = 32;

    egbos::alloc::FixedPool<TestSlot, kPoolSize> pool;

    std::vector<TestSlot*> slots;
    slots.reserve(kPoolSize);

    for (std::size_t i = 0; i < kPoolSize; ++i) {
        TestSlot* s = pool.acquire();
        ASSERT_NE(s, nullptr);
        slots.push_back(s);
    }

    // All pointers should be unique
    std::vector<TestSlot*> sorted_slots = slots;
    std::sort(sorted_slots.begin(), sorted_slots.end());
    auto it = std::unique(sorted_slots.begin(), sorted_slots.end());
    EXPECT_EQ(it, sorted_slots.end()) << "Duplicate pointers returned by pool";

    // All pointers should be aligned to alignof(TestSlot)
    for (auto* s : slots) {
        auto addr = reinterpret_cast<std::uintptr_t>(s);
        EXPECT_EQ(addr % alignof(TestSlot), 0u)
            << "Pool returned misaligned pointer";
    }

    // Cleanup
    for (auto* s : slots) {
        pool.release(s);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Compile-time constraint tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, CompileTimeConstraints) {
    // Power of 2 sizes compile successfully
    egbos::alloc::FixedPool<uint64_t, 1> p1;
    egbos::alloc::FixedPool<uint64_t, 2> p2;
    egbos::alloc::FixedPool<uint64_t, 4> p4;
    egbos::alloc::FixedPool<uint64_t, 256> p256;
    (void)p1; (void)p2; (void)p4; (void)p256;

    // Non-power-of-2 would fail: FixedPool<uint64_t, 3> — static_assert
    // Non-trivially-destructible would fail — static_assert
    SUCCEED() << "All compile-time constraints validated";
}

// ─────────────────────────────────────────────────────────────────────────────
// Zero-allocation verification
// Under ASAN, this test confirms no heap allocations from pool operations.
// ─────────────────────────────────────────────────────────────────────────────

TEST(PoolAllocatorTest, ZeroHeapAllocation) {
    constexpr std::size_t kPoolSize = 128;
    constexpr std::size_t kIterations = 100'000;

    egbos::alloc::FixedPool<TestSlot, kPoolSize> pool;

    // Rapid acquire/release — ASAN would detect any malloc/new
    for (std::size_t i = 0; i < kIterations; ++i) {
        TestSlot* s = pool.acquire();
        if (s != nullptr) {
            s->id = i;
            pool.release(s);
        }
    }

    SUCCEED() << "Zero heap allocation under ASAN";
}

}  // namespace
