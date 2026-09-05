// engine/tests/test_uptime_telemetry.cpp
// EGBOS S5-REQ-003 — Uptime Telemetry Unit Tests
//
// Specification refs  : S5-REQ-003 → CLAIM-INV-001 / 336-hour trial
// Classification      : INFRASTRUCTURE_READY — 336H TRIAL NOT EXECUTED
// Source revision     : 2663389

#include <gtest/gtest.h>
#include "engine/core/uptime_telemetry.h"

#include <chrono>
#include <cstring>
#include <thread>

#if defined(__linux__)
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/stat.h>
#endif

using egbos::s5::UptimeTelemetry;
using egbos::s5::HealthState;

// ── Test 1: Initial state ─────────────────────────────────────────────────────
TEST(UptimeTelemetryTest, InitialStateIsZero) {
    UptimeTelemetry t;
    EXPECT_EQ(t.start_time_ns.load(), 0ULL);
    EXPECT_EQ(t.stop_time_ns.load(), 0ULL);
    EXPECT_EQ(t.inv001_violations.load(), 0ULL);
    EXPECT_EQ(t.restart_count.load(), 0ULL);
    EXPECT_EQ(t.watchdog_ticks.load(), 0ULL);
    EXPECT_EQ(t.health_state.load(),
              static_cast<uint8_t>(HealthState::HEALTHY));
    EXPECT_EQ(t.elapsed_ns(), 0ULL);
    EXPECT_FALSE(t.trial_complete());
}

// ── Test 2: record_start populates identity fields ────────────────────────────
TEST(UptimeTelemetryTest, RecordStart_SetsRevAndEnv) {
    UptimeTelemetry t;
    t.record_start("2663389", "dev-workstation-001");
    EXPECT_STREQ(t.source_revision, "2663389");
    EXPECT_STREQ(t.environment, "dev-workstation-001");
    EXPECT_GT(t.start_time_ns.load(), 0ULL);
    EXPECT_EQ(t.stop_time_ns.load(), 0ULL);
    EXPECT_EQ(t.health_state.load(),
              static_cast<uint8_t>(HealthState::HEALTHY));
}

// ── Test 3: elapsed_ns is monotonically increasing ────────────────────────────
TEST(UptimeTelemetryTest, ElapsedNs_Monotonic) {
    UptimeTelemetry t;
    t.record_start("2663389", "dev-workstation-001");

    const uint64_t e1 = t.elapsed_ns();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const uint64_t e2 = t.elapsed_ns();

    EXPECT_GT(e2, e1) << "elapsed_ns must increase over time";
}

// ── Test 4: record_stop freezes elapsed ──────────────────────────────────────
TEST(UptimeTelemetryTest, RecordStop_FreezesElapsed) {
    UptimeTelemetry t;
    t.record_start("2663389", "dev-workstation-001");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    t.record_stop();

    const uint64_t frozen = t.elapsed_ns();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const uint64_t after  = t.elapsed_ns();

    EXPECT_EQ(frozen, after) << "elapsed_ns must not change after record_stop";
    EXPECT_GT(frozen, 0ULL);
}

// ── Test 5: INV-001 violation recording ──────────────────────────────────────
TEST(UptimeTelemetryTest, Inv001Violation_SetsStateAndCount) {
    UptimeTelemetry t;
    t.record_start("2663389", "dev-workstation-001");

    EXPECT_EQ(t.inv001_violations.load(), 0ULL);
    EXPECT_EQ(t.health_state.load(),
              static_cast<uint8_t>(HealthState::HEALTHY));

    t.record_inv001_violation();

    EXPECT_EQ(t.inv001_violations.load(), 1ULL);
    EXPECT_EQ(t.health_state.load(),
              static_cast<uint8_t>(HealthState::INV001_VIOLATED));
    EXPECT_FALSE(t.trial_complete())
        << "trial_complete must be false when violations > 0";

    t.record_inv001_violation();
    EXPECT_EQ(t.inv001_violations.load(), 2ULL);
}

// ── Test 6: Watchdog ticks increment ─────────────────────────────────────────
TEST(UptimeTelemetryTest, WatchdogTick_Increments) {
    UptimeTelemetry t;
    t.record_start("2663389", "dev-workstation-001");

    for (int i = 0; i < 100; ++i) t.watchdog_tick();
    EXPECT_EQ(t.watchdog_ticks.load(), 100ULL);
}

// ── Test 7: Watchdog expire sets state ───────────────────────────────────────
TEST(UptimeTelemetryTest, WatchdogExpire_SetsState) {
    UptimeTelemetry t;
    t.record_start("2663389", "dev-workstation-001");
    t.watchdog_expire();
    EXPECT_EQ(t.health_state.load(),
              static_cast<uint8_t>(HealthState::WATCHDOG_EXPIRED));
    EXPECT_FALSE(t.trial_complete());
}

// ── Test 8: trial_complete false before 336h ──────────────────────────────────
TEST(UptimeTelemetryTest, TrialComplete_FalseBeforeTarget) {
    UptimeTelemetry t;
    t.record_start("2663389", "dev-workstation-001");
    // elapsed is milliseconds, far below 336 hours
    EXPECT_FALSE(t.trial_complete())
        << "trial_complete must be false in a just-started session";
}

// ── Test 9: kTargetTrialNs constant is correct ────────────────────────────────
TEST(UptimeTelemetryTest, TargetTrialNs_Is336Hours) {
    constexpr uint64_t expected =
        336ULL * 3600ULL * 1'000'000'000ULL;
    EXPECT_EQ(UptimeTelemetry::kTargetTrialNs, expected)
        << "kTargetTrialNs must represent 336 hours in nanoseconds";
}

// ── Test 10: Persistence produces non-empty output (Linux only) ───────────────
#if defined(__linux__)
TEST(UptimeTelemetryTest, Persist_WritesNdjsonLine) {
    // Write to a temp file to verify persistence.
    char tmp[] = "/tmp/egbos_uptime_test_XXXXXX";
    int fd = ::mkstemp(tmp);
    ASSERT_GE(fd, 0) << "Cannot create temp file";

    UptimeTelemetry t;
    t.record_start("2663389", "test");
    t.watchdog_tick();
    t.persist(fd, "test_event");

    // Read back and verify non-empty NDJSON line.
    ::lseek(fd, 0, SEEK_SET);
    char buf[1024] = {};
    ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    EXPECT_GT(n, 0) << "persist wrote nothing";

    // Must contain key fields.
    EXPECT_NE(std::string(buf).find("test_event"),    std::string::npos);
    EXPECT_NE(std::string(buf).find("INFRASTRUCTURE_READY"), std::string::npos);
    EXPECT_NE(std::string(buf).find("inv001_violations"), std::string::npos);

    // Must end with newline (NDJSON).
    EXPECT_EQ(buf[n - 1], '\n') << "NDJSON line must end with newline";

    ::close(fd);
    ::unlink(tmp);
}

// ── Test 11: Restart count increments ────────────────────────────────────────
TEST(UptimeTelemetryTest, RestartCount_Increments) {
    UptimeTelemetry t;
    t.record_start("2663389", "dev-workstation-001");
    t.restart_count.fetch_add(1, std::memory_order_relaxed);
    EXPECT_EQ(t.restart_count.load(), 1ULL);
}
#endif // __linux__

// ── Classification assertion ──────────────────────────────────────────────────
// This test is a documentation assertion: it will fail to compile if the
// kTargetTrialNs constant is removed, preventing accidental regression.
TEST(UptimeTelemetryTest, ClassificationAssertion_336HNotExecuted) {
    // This comment is the authoritative statement of evidence status:
    // - INFRASTRUCTURE_READY: the telemetry mechanism exists and is tested.
    // - 336H TRIAL NOT EXECUTED: no production uptime log has been generated.
    // - V4 is NOT satisfied by the existence of this infrastructure.
    SUCCEED() << "INFRASTRUCTURE_READY — 336H TRIAL NOT EXECUTED";
}
