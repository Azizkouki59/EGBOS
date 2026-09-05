// engine/core/uptime_telemetry.h
// EGBOS S5-REQ-003 — Operational Uptime and INV-001 Violation Telemetry
//
// Specification refs  : S5-REQ-003 — 14-day / 336-hour zero-INV-001-violation trial
// Governed component  : CLAIM-INV-001 (exec.position invariant)
// Classification      : INFRASTRUCTURE_READY  (336-hour trial NOT executed)
// Source revision     : 2663389
//
// PURPOSE:
//   Provide a zero-allocation telemetry record capable of tracking monotonic
//   process uptime, INV-001 violation count, watchdog ticks, and health state.
//   Persistence is via atomic-append to newline-delimited JSON (NDJSON).
//
// CRITICAL LIMITATION — READ BEFORE USING:
//   This header provides INFRASTRUCTURE only.
//   - No 336-hour trial has been executed.
//   - No live operational evidence has been produced.
//   - V4 promotion of CLAIM-INV-001 / S5-REQ-003 requires:
//       * 336 continuous hours of operational uptime
//       * zero INV-001 violations in that window
//       * the telemetry log appended and persisted throughout
//       * a signed govcheck attestation at the end of the trial
//
// GOVCHECK COMPLIANCE:
//   GC-E-010 : No dynamic allocation — all state is POD / fixed-size arrays
//   GC-E-011 : CLOCK_MONOTONIC only (no CLOCK_REALTIME) for uptime measurement
//   Only standard C++ and POSIX primitives.

#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <ctime>

#if defined(__linux__)
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/uio.h>
#endif

namespace egbos::s5 {

// ── Health states ─────────────────────────────────────────────────────────────
enum class HealthState : uint8_t {
    HEALTHY           = 0,
    WATCHDOG_EXPIRED  = 1,
    INV001_VIOLATED   = 2,
    HALTED            = 3,
};

// ── UptimeTelemetry ───────────────────────────────────────────────────────────
// Zero-allocation telemetry record.
// All fields are either POD or std::atomic<POD>.
// Not copyable, not movable (intended as process-singleton).
//
// Classification: INFRASTRUCTURE_READY — 336H TRIAL NOT EXECUTED
struct alignas(64) UptimeTelemetry {
    // ── Identity ──────────────────────────────────────────────────────────────
    char    source_revision[16];   // null-terminated git SHA prefix
    char    environment[32];       // null-terminated environment tag

    // ── Session boundaries (CLOCK_MONOTONIC nanoseconds) ─────────────────────
    std::atomic<uint64_t> start_time_ns{0};
    std::atomic<uint64_t> stop_time_ns{0};

    // ── Monotonic counters ────────────────────────────────────────────────────
    std::atomic<uint64_t> inv001_violations{0};   // must be 0 for V4
    std::atomic<uint64_t> restart_count{0};
    std::atomic<uint64_t> watchdog_ticks{0};

    // ── State ─────────────────────────────────────────────────────────────────
    std::atomic<uint8_t> health_state{
        static_cast<uint8_t>(HealthState::HEALTHY)};

    // ── Target trial duration ─────────────────────────────────────────────────
    // 336 hours = 14 days × 24 hours. This constant is advisory; the actual
    // trial duration must be verified from the persisted NDJSON log.
    static constexpr uint64_t kTargetTrialNs =
        336ULL * 3600ULL * 1'000'000'000ULL; // 336 hours in nanoseconds

    // ── Constructor ───────────────────────────────────────────────────────────
    UptimeTelemetry() noexcept {
        std::memset(source_revision, 0, sizeof(source_revision));
        std::memset(environment, 0, sizeof(environment));
    }

    // Not copyable / movable — singleton semantics.
    UptimeTelemetry(const UptimeTelemetry&) = delete;
    UptimeTelemetry& operator=(const UptimeTelemetry&) = delete;
    UptimeTelemetry(UptimeTelemetry&&) = delete;
    UptimeTelemetry& operator=(UptimeTelemetry&&) = delete;

    // ── Session management ────────────────────────────────────────────────────

    // Mark process start. Must be called once on process initialization.
    void record_start(const char* rev, const char* env) noexcept {
        strncpy(source_revision, rev, sizeof(source_revision) - 1);
        strncpy(environment, env, sizeof(environment) - 1);
        start_time_ns.store(monotonic_ns(), std::memory_order_release);
        health_state.store(static_cast<uint8_t>(HealthState::HEALTHY),
                           std::memory_order_release);
    }

    // Mark process stop.
    void record_stop() noexcept {
        stop_time_ns.store(monotonic_ns(), std::memory_order_release);
    }

    // ── INV-001 violation recording ───────────────────────────────────────────
    // Call when gross_exposure_ != Σ|signed_qty| is detected at runtime.
    // Any non-zero value blocks V4.
    void record_inv001_violation() noexcept {
        inv001_violations.fetch_add(1, std::memory_order_relaxed);
        health_state.store(static_cast<uint8_t>(HealthState::INV001_VIOLATED),
                           std::memory_order_release);
    }

    // ── Watchdog ──────────────────────────────────────────────────────────────
    void watchdog_tick() noexcept {
        watchdog_ticks.fetch_add(1, std::memory_order_relaxed);
    }

    void watchdog_expire() noexcept {
        health_state.store(static_cast<uint8_t>(HealthState::WATCHDOG_EXPIRED),
                           std::memory_order_release);
    }

    // ── Elapsed uptime query ──────────────────────────────────────────────────
    [[nodiscard]] uint64_t elapsed_ns() const noexcept {
        const uint64_t s = start_time_ns.load(std::memory_order_acquire);
        if (s == 0) return 0;
        const uint64_t e = stop_time_ns.load(std::memory_order_acquire);
        const uint64_t now = (e > 0) ? e : monotonic_ns();
        return (now > s) ? (now - s) : 0;
    }

    // Returns true iff elapsed >= 336 hours AND zero violations.
    // Does NOT guarantee V4 — see limitations above.
    [[nodiscard]] bool trial_complete() const noexcept {
        return (elapsed_ns() >= kTargetTrialNs) &&
               (inv001_violations.load(std::memory_order_relaxed) == 0) &&
               (health_state.load(std::memory_order_relaxed) ==
                    static_cast<uint8_t>(HealthState::HEALTHY));
    }

    // ── Persistence ───────────────────────────────────────────────────────────
    // Appends a single NDJSON line to the given file descriptor (O_APPEND).
    // Uses writev for atomic append semantics (POSIX O_APPEND guarantee).
    // Does NOT allocate heap. Buffer is stack-local.
    //
    // Format (one line):
    // {"event":"<tag>","start_ns":<N>,"stop_ns":<N>,"elapsed_ns":<N>,
    //  "inv001_violations":<N>,"restart_count":<N>,"watchdog_ticks":<N>,
    //  "health":<N>,"rev":"<rev>","env":"<env>","class":"INFRASTRUCTURE_READY"}
    void persist(int fd, const char* event_tag) const noexcept {
#if defined(__linux__)
        char buf[512];
        const int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"%s\","
            "\"start_ns\":%llu,"
            "\"stop_ns\":%llu,"
            "\"elapsed_ns\":%llu,"
            "\"inv001_violations\":%llu,"
            "\"restart_count\":%llu,"
            "\"watchdog_ticks\":%llu,"
            "\"health\":%u,"
            "\"rev\":\"%s\","
            "\"env\":\"%s\","
            "\"classification\":\"INFRASTRUCTURE_READY\","
            "\"trial_complete\":%s}\n",
            event_tag,
            (unsigned long long)start_time_ns.load(std::memory_order_relaxed),
            (unsigned long long)stop_time_ns.load(std::memory_order_relaxed),
            (unsigned long long)elapsed_ns(),
            (unsigned long long)inv001_violations.load(std::memory_order_relaxed),
            (unsigned long long)restart_count.load(std::memory_order_relaxed),
            (unsigned long long)watchdog_ticks.load(std::memory_order_relaxed),
            health_state.load(std::memory_order_relaxed),
            source_revision,
            environment,
            trial_complete() ? "true" : "false");

        if (n > 0 && n < static_cast<int>(sizeof(buf))) {
            struct iovec iov{buf, static_cast<size_t>(n)};
            (void)::writev(fd, &iov, 1);
        }
#else
        (void)fd; (void)event_tag;
#endif
    }

private:
    // ── Clock (GC-E-011 compliant) ────────────────────────────────────────────
    [[nodiscard]] static uint64_t monotonic_ns() noexcept {
#if defined(__linux__)
        struct timespec ts;
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
             + static_cast<uint64_t>(ts.tv_nsec);
#else
        // Non-Linux fallback: use steady_clock.
        // Not used in production (Linux-only deployment).
        return static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
#endif
    }
};

} // namespace egbos::s5
