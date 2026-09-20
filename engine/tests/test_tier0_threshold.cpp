#include <gtest/gtest.h>

#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"

#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>

using namespace egbos::l0;
using namespace egbos::shm;

namespace {

std::atomic<int> g_fsync_calls{0};
std::atomic<int> g_fsync_fail{0};

const char* kJson =
    R"({"e":"depthUpdate","U":100,"u":100,"b":[["1.0","1.0"]],"a":[]})";

int open_temp_tier0() {
    char path[] = "/tmp/egbos_tier0_threshold_XXXXXX";
    const int fd = ::mkstemp(path);
    if (fd >= 0) {
        ::unlink(path);
    }
    return fd;
}

void reset_fsync_mock() {
    g_fsync_calls.store(0);
    g_fsync_fail.store(0);
}

} // namespace

extern "C" int __wrap_fsync(int fd) {
    g_fsync_calls.fetch_add(1);
    if (g_fsync_fail.load() != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

TEST(Tier0ThresholdTest, DoesNotFlushBeforeFiveMsOr64KB) {
    static MarketEventsRegion ring{};
    GovernanceFlags gov{};

    const int fd = open_temp_tier0();
    ASSERT_GE(fd, 0);

    reset_fsync_mock();

    FeedIngestor ingestor(&ring, &gov, fd);

    ingestor.process_payload(
        reinterpret_cast<const uint8_t*>(kJson),
        std::strlen(kJson),
        1'000'000'000ULL
    );

    EXPECT_EQ(g_fsync_calls.load(), 0);

    ingestor.check_staleness(1'004'000'000ULL);

    EXPECT_EQ(g_fsync_calls.load(), 0);

    ::close(fd);
}

TEST(Tier0ThresholdTest, FlushesAtFiveMsDuringIdleCheck) {
    static MarketEventsRegion ring{};
    GovernanceFlags gov{};

    const int fd = open_temp_tier0();
    ASSERT_GE(fd, 0);

    reset_fsync_mock();

    FeedIngestor ingestor(&ring, &gov, fd);

    ingestor.process_payload(
        reinterpret_cast<const uint8_t*>(kJson),
        std::strlen(kJson),
        1'000'000'000ULL
    );

    EXPECT_EQ(g_fsync_calls.load(), 0);

    // No new market-data frame arrived; check_staleness services AD-003.
    ingestor.check_staleness(1'005'000'000ULL);

    EXPECT_EQ(g_fsync_calls.load(), 1);

    ::close(fd);
}

TEST(Tier0ThresholdTest, FlushesAt64KB) {
    static MarketEventsRegion ring{};
    GovernanceFlags gov{};

    const int fd = open_temp_tier0();
    ASSERT_GE(fd, 0);

    reset_fsync_mock();

    FeedIngestor ingestor(&ring, &gov, fd);

    constexpr int kRecordsTo64KB = 65536 / 128;

    for (int i = 0; i < kRecordsTo64KB; ++i) {
        ingestor.process_payload(
            reinterpret_cast<const uint8_t*>(kJson),
            std::strlen(kJson),
            2'000'000'000ULL
        );
    }

    EXPECT_EQ(g_fsync_calls.load(), 1);

    ::close(fd);
}

TEST(Tier0ThresholdTest, FsyncFailurePreventsSpscPublication) {
    static MarketEventsRegion ring{};
    GovernanceFlags gov{};

    const int fd = open_temp_tier0();
    ASSERT_GE(fd, 0);

    reset_fsync_mock();
    g_fsync_fail.store(1);

    FeedIngestor ingestor(&ring, &gov, fd);

    constexpr int kRecordsTo64KB = 65536 / 128;

    for (int i = 0; i < kRecordsTo64KB; ++i) {
        ingestor.process_payload(
            reinterpret_cast<const uint8_t*>(kJson),
            std::strlen(kJson),
            3'000'000'000ULL
        );
    }

    EXPECT_EQ(g_fsync_calls.load(), 1);

    int popped = 0;
    MarketEventSlot slot{};
    while (ring.pop(slot)) {
        ++popped;
    }

    // The record that triggered the failed flush must not be published.
    EXPECT_EQ(popped, kRecordsTo64KB - 1);

    ::close(fd);
}

// ── Regression: Tier-0 fault must not be cleared by process_payload() ────────
//
// Context: BinanceFeedListener::run_loop() now calls (2026-09-20 fix):
//
//   ingestor_.check_staleness(rx_timestamp_ns);   // step 3
//   ingestor_.process_payload(...);               // step 6
//
// If check_staleness() detects a Tier-0 fsync failure it sets FEED_STALE and
// returns.  process_payload() must NOT then transition the feed back to
// FEED_NORMAL (which was the pre-fix behavior for ALL non-NORMAL states).
//
// The fix: process_payload() only clears state when !tier0_fault_.
// tier0_fault_ is a sticky bool set in tier0_flush_if_due() on fsync error.
//
// Test sequence:
//   1. Write one record so tier0_pending_bytes_ > 0.
//   2. Arm fsync failure mock.
//   3. Call check_staleness() at T0+5ms → fsync fires, fails → tier0_fault_=true
//      → current_state_ = FEED_STALE.
//   4. Call process_payload() with a valid frame (mirrors the new run_loop order).
//   5. Verify SPSC ring remains empty — Tier-0 fault blocks publication and
//      FEED_STALE was not silently cleared.
//
// Classification: LOCAL_SYNTHETIC / REG-TIER0-FAULT-001
TEST(Tier0ThresholdTest, FsyncFailure_FeedStaleNotClearedByProcessPayload) {
    static MarketEventsRegion ring{};
    GovernanceFlags gov{};

    const int fd = open_temp_tier0();
    ASSERT_GE(fd, 0);

    reset_fsync_mock();

    FeedIngestor ingestor(&ring, &gov, fd);

    constexpr uint64_t T0 = 1'000'000'000ULL;
    constexpr uint64_t T1 = T0 + 5'000'001ULL; // T0 + just over 5 ms → time_due

    // Step 1: write one record at T0 to accumulate pending Tier-0 bytes.
    ingestor.process_payload(
        reinterpret_cast<const uint8_t*>(kJson),
        std::strlen(kJson),
        T0
    );
    // Drain any events published before the fault.
    MarketEventSlot slot{};
    while (ring.pop(slot)) {}

    // Step 2: arm the fsync failure mock.
    g_fsync_fail.store(1);

    // Step 3: trigger the time-due flush path via check_staleness().
    // tier0_flush_if_due() will call __wrap_fsync(), which returns -1/EIO.
    // Expected result: tier0_fault_=true, current_state_=FEED_STALE.
    ingestor.check_staleness(T1);

    EXPECT_GE(g_fsync_calls.load(), 1)
        << "fsync must have been called by check_staleness at T+5ms";

    // Step 4: clear the fsync failure mock, simulating a transient storage
    // failure that recovers.
    g_fsync_fail.store(0);

    // Step 5: a new valid frame arrives — exactly the post-fix run_loop order:
    //   check_staleness(T1) was step 3; process_payload() is step 6.
    // This must NOT clear the Tier-0 fault state back to FEED_NORMAL, and
    // it must NOT publish the new frame.
    ingestor.process_payload(
        reinterpret_cast<const uint8_t*>(kJson),
        std::strlen(kJson),
        T1
    );

    // Step 6: the ring must remain empty.
    // Since tier0_fault_ is permanently sticky, tier0_append() must return false
    // immediately, blocking publication even though fsync() would now succeed.
    // The key requirement is I-S2-001: no SPSC publication after a missing
    // Tier-0 log entry.
    bool any_published = false;
    while (ring.pop(slot)) { any_published = true; }
    EXPECT_FALSE(any_published)
        << "Tier-0 fault must permanently block SPSC publication on subsequent frames; "
           "FEED_STALE must not be cleared even if storage recovers";

    ::close(fd);
}
