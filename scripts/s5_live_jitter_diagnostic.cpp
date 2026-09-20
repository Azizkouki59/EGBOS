// scripts/s5_live_binance_qualification.cpp
// EGBOS S5 — Live Binance Feed Qualification Harness
//
// Specification  : S5-REQ-001 (Live Feed Integration), MARKET-DATA-001
// Governance     : EGBOS-GOV-DECISION-2026-009 (D4, D7, D9)
//                  EGBOS-GOV-DECISION-2026-010 (GOV-S5-NETWORK-01 — Wi-Fi authorization)
// Verification   : V4 — Empirical Validation on production feed
//
// EVIDENCE_CLASS : LIVE_OPERATIONAL — ONLY when connected to stream.binance.com
//                  and not running under a disqualified environment.
//
// This harness:
//   1. Detects the execution environment (WSL2 vs bare-metal Tier-0).
//   2. Connects to the real Binance production WebSocket endpoint via TLS.
//   3. Registers a frame-arrival callback on BinanceFeedListener that records
//      every received frame's CLOCK_MONOTONIC_RAW timestamp via the canonical
//      set_frame_arrival_callback() API.
//   4. Computes per-frame latency (inter-frame jitter proxy) and aggregate stats.
//   5. Writes a structured JSON evidence artifact.
//   6. Writes a separate SHA256 sidecar over the artifact bytes.
//
// Governance invariants (enforced at runtime):
//   D4  : All timestamps from CLOCK_MONOTONIC_RAW only.
//   D7  : FeedProvenance is read directly from BinanceFeedListener.
//   D9  : Evidence marked DISQUALIFIED_HOST if WSL2 detected.
//   D9  : No synthetic/loopback traffic used.
//   GC-E-010 : No dynamic allocation on the hot receive path.
//   GC-E-011 : No CLOCK_REALTIME on measurement paths.

#include "engine/l0/binance_feed_listener.h"
#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <thread>

#if defined(__linux__)
#include <openssl/evp.h>   // EVP_MD_CTX, EVP_sha256
#include <sys/utsname.h>   // uname
#include <unistd.h>
#endif

using namespace egbos::l0;

// ─────────────────────────────────────────────────────────────────────────────
// D4: CLOCK_MONOTONIC_RAW helper (same as BinanceFeedListener)
// ─────────────────────────────────────────────────────────────────────────────
static uint64_t mono_raw_ns() noexcept {
#if defined(__linux__)
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
#else
    return 0ULL;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Wall-clock ISO-8601 timestamp for artifact metadata (NOT for measurements)
// ─────────────────────────────────────────────────────────────────────────────
static void utc_iso8601(char* buf, size_t n) noexcept {
    time_t t = time(nullptr);
    struct tm tm_utc{};
    gmtime_r(&t, &tm_utc);
    strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

// ─────────────────────────────────────────────────────────────────────────────
// Environment detection
// WSL2 is disqualified from producing LIVE_OPERATIONAL evidence for S5-AC-001
// because it lacks isolcpus/nohz_full Tier-0 kernel parameters.
// ─────────────────────────────────────────────────────────────────────────────
static bool is_wsl2() noexcept {
#if defined(__linux__)
    struct utsname u{};
    if (::uname(&u) != 0) return false;
    return (::strstr(u.release, "WSL2") != nullptr ||
            ::strstr(u.release, "microsoft") != nullptr);
#else
    return false;
#endif
}

static void get_kernel_release(char* buf, size_t n) noexcept {
#if defined(__linux__)
    struct utsname u{};
    if (::uname(&u) == 0) {
        ::snprintf(buf, n, "%s", u.release);
        return;
    }
#endif
    ::snprintf(buf, n, "unknown");
}

#include <vector>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Jitter metrics — updated from the frame-arrival callback.
// All fields are std::atomic so the listener thread and the main thread can
// read them safely without a lock.
// ─────────────────────────────────────────────────────────────────────────────
struct Metrics {
    std::atomic<uint64_t> frames{0};
    std::atomic<uint64_t> sum_interval_ns{0};
    std::atomic<uint64_t> min_interval_ns{UINT64_MAX};
    std::atomic<uint64_t> max_interval_ns{0};
    // prev_rx_ns is written only by the listener thread — no atomic needed,
    // but use atomic for simplicity to avoid any UB if ever moved off-thread.
    std::atomic<uint64_t> prev_rx_ns{0};
    std::atomic<bool>     first_frame{true};
    
    // Diagnostic additions to record distribution without allocations
    static constexpr size_t kMaxIntervals = 500000;
    uint64_t* intervals = nullptr;
    std::atomic<size_t> num_intervals{0};
    
    Metrics() {
        intervals = new uint64_t[kMaxIntervals]{};
    }
    ~Metrics() {
        delete[] intervals;
    }
};

// Returns a callback suitable for set_frame_arrival_callback().
// Captures metrics by pointer (metrics lifetime must exceed the listener).
static std::function<void(uint64_t)> make_frame_callback(Metrics& m) {
    return [&m](uint64_t rx_ns) noexcept {
        m.frames.fetch_add(1, std::memory_order_relaxed);

        bool expected_first = true;
        if (m.first_frame.compare_exchange_strong(expected_first, false,
                                                   std::memory_order_relaxed)) {
            // Record first timestamp; no interval yet.
            m.prev_rx_ns.store(rx_ns, std::memory_order_relaxed);
            return;
        }

        uint64_t prev = m.prev_rx_ns.exchange(rx_ns, std::memory_order_relaxed);
        uint64_t interval = (rx_ns > prev) ? (rx_ns - prev) : 0;

        m.sum_interval_ns.fetch_add(interval, std::memory_order_relaxed);

        // min
        uint64_t old_min = m.min_interval_ns.load(std::memory_order_relaxed);
        while (interval < old_min &&
               !m.min_interval_ns.compare_exchange_weak(old_min, interval,
                   std::memory_order_relaxed)) {}
        // max
        uint64_t old_max = m.max_interval_ns.load(std::memory_order_relaxed);
        while (interval > old_max &&
               !m.max_interval_ns.compare_exchange_weak(old_max, interval,
                   std::memory_order_relaxed)) {}
                   
        size_t idx = m.num_intervals.fetch_add(1, std::memory_order_relaxed);
        if (idx < Metrics::kMaxIntervals) {
            m.intervals[idx] = interval;
        }
    };
}

// Write evidence removed for diagnostic test
// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
#if defined(__linux__)
    ::signal(SIGPIPE, SIG_IGN);
#endif
    static constexpr int    kQualDurationSec = 120;

    int duration_sec = kQualDurationSec;
    if (argc >= 2) duration_sec = ::atoi(argv[1]);
    if (duration_sec <= 0) duration_sec = kQualDurationSec;

    // ── Environment detection ────────────────────────────────────────────────
    const bool wsl2_env = is_wsl2();
    char kernel_rel[128]{};
    get_kernel_release(kernel_rel, sizeof(kernel_rel));

    std::cout << "==================================================\n"
              << " EGBOS Live Binance Jitter Diagnostic\n"
              << "==================================================\n"
              << "Kernel        : " << kernel_rel << "\n"
              << "Environment   : " << (wsl2_env ? "WSL2" : "Bare-metal") << "\n"
              << "Endpoint      : wss://stream.binance.com:9443/ws\n"
              << "Duration      : " << duration_sec << "s\n"
              << "Timestamp src : CLOCK_MONOTONIC_RAW (D4)\n"
              << "------------------------------------------------\n";

    // ── SHM regions (stack allocation for qualification — no production SHM) ──
    static egbos::shm::MarketEventsRegion s_market_events{};
    static egbos::shm::GovernanceFlags    s_gov_flags{};

    // ── Metrics — populated via frame-arrival callback ───────────────────────
    Metrics metrics{};

    // ── FeedIngestor ─────────────────────────────────────────────────────────
    FeedIngestor ingestor(&s_market_events, &s_gov_flags, /*tier0_fd=*/-1);

    // ── BinanceFeedListener configuration ────────────────────────────────────
    BinanceFeedConfig config;
    config.use_testnet    = false;                    // Production endpoint (D9)
    config.host           = "stream.binance.com";
    config.port           = 9443;
    config.path           = "/ws/btcusdt@depth@100ms"; // High-density 100ms stream
    config.auto_reconnect = false;

    BinanceFeedListener listener(ingestor, config);

    // ── Register frame-arrival callback ──────────────────────────────────────
    // This is the corrected instrumentation path: the callback is invoked from
    // within run_loop() immediately after process_payload(), using the same
    // CLOCK_MONOTONIC_RAW timestamp already captured at frame receive.
    listener.set_frame_arrival_callback(make_frame_callback(metrics));

    std::cout << "Provenance    : " << listener.provenance_str() << "\n\n"
              << "Connecting to wss://stream.binance.com:9443/ws ...\n";

    // ── Record start time ─────────────────────────────────────────────────────
    char started_at[32]{};
    utc_iso8601(started_at, sizeof(started_at));
    const uint64_t t_start_ns = mono_raw_ns();

    // ── Listener thread ───────────────────────────────────────────────────────
    std::thread listener_thread([&listener]() {
        listener.run_loop();
    });

    // Main thread waits for the measurement duration, then stops the listener.
    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
    listener.request_stop();
    listener_thread.join();

    // ── Record end time ───────────────────────────────────────────────────────
    const uint64_t t_end_ns = mono_raw_ns();
    char ended_at[32]{};
    utc_iso8601(ended_at, sizeof(ended_at));
    const uint64_t duration_ns = t_end_ns - t_start_ns;

    // ── Connection success ────────────────────────────────────────────────────
    const ListenerState final_state = listener.state();
    const bool connection_success =
        (final_state != ListenerState::NON_OPERATIONAL_MULTICAST_FAILED &&
         final_state != ListenerState::FAULTED);

    // ── Compute aggregate statistics ──────────────────────────────────────────
    const uint64_t n_frames   = metrics.frames.load(std::memory_order_relaxed);
    const uint64_t sum_ns     = metrics.sum_interval_ns.load(std::memory_order_relaxed);
    const uint64_t min_ns     = metrics.min_interval_ns.load(std::memory_order_relaxed);
    const uint64_t max_ns     = metrics.max_interval_ns.load(std::memory_order_relaxed);
    const uint64_t intervals  = (n_frames > 1) ? (n_frames - 1) : 1;
    const uint64_t mean_ns    = (n_frames > 1) ? (sum_ns / intervals) : 0;
    const uint64_t jitter_ns  = (max_ns > min_ns && min_ns != UINT64_MAX) ? (max_ns - min_ns) : 0;

    uint64_t p50_ns = 0, p95_ns = 0, p99_ns = 0;
    size_t count = metrics.num_intervals.load(std::memory_order_relaxed);
    if (count > Metrics::kMaxIntervals) count = Metrics::kMaxIntervals;
    
    if (count > 0) {
        std::sort(metrics.intervals, metrics.intervals + count);
        p50_ns = metrics.intervals[static_cast<size_t>(count * 0.50)];
        p95_ns = metrics.intervals[static_cast<size_t>(count * 0.95)];
        p99_ns = metrics.intervals[static_cast<size_t>(count * 0.99)];
    }

    std::cout << "\n------------------------------------------------\n"
              << "Results:\n"
              << "  Duration    : " << duration_ns / 1'000'000ULL << " ms\n"
              << "  Frames recv : " << n_frames << "\n"
              << "  Conn success: " << (connection_success ? "YES" : "NO") << "\n"
              << "  Min inter-frame : " << (min_ns == UINT64_MAX ? 0 : min_ns) / 1'000'000.0 << " ms\n"
              << "  Max inter-frame : " << max_ns / 1'000'000.0 << " ms\n"
              << "  Mean inter-frame: " << mean_ns / 1'000'000.0 << " ms\n"
              << "  p50  inter-frame: " << p50_ns / 1'000'000.0 << " ms\n"
              << "  p95  inter-frame: " << p95_ns / 1'000'000.0 << " ms\n"
              << "  p99  inter-frame: " << p99_ns / 1'000'000.0 << " ms\n"
              << "  Jitter (p2p)    : " << jitter_ns / 1'000'000.0 << " ms\n"
              << "------------------------------------------------\n\n";

    if (!connection_success) {
        std::cout << "[ERROR] Connection FAILED.\n";
        return 3;
    }

    if (n_frames == 0) {
        std::cout << "[ERROR] Connection succeeded but ZERO frames received.\n";
        return 4;
    }

    return 0;
}
