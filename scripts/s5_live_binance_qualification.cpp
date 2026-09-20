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
#include <memory>

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

// ─────────────────────────────────────────────────────────────────────────────
// SHA256 over a file's bytes (sidecar pattern — no circularity)
// ─────────────────────────────────────────────────────────────────────────────
static bool sha256_file(const char* path, char* hex_out, size_t hex_len) noexcept {
#if defined(__linux__)
    FILE* f = ::fopen(path, "rb");
    if (!f) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) { ::fclose(f); return false; }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx); ::fclose(f); return false;
    }

    uint8_t chunk[4096];
    size_t nr;
    while ((nr = ::fread(chunk, 1, sizeof(chunk), f)) > 0) {
        EVP_DigestUpdate(ctx, chunk, nr);
    }
    ::fclose(f);

    uint8_t digest[32];
    unsigned int dlen = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &dlen) != 1) {
        EVP_MD_CTX_free(ctx); return false;
    }
    EVP_MD_CTX_free(ctx);

    if (hex_len < 65) return false;
    static const char hx[] = "0123456789abcdef";
    for (unsigned i = 0; i < 32; ++i) {
        hex_out[i * 2]     = hx[digest[i] >> 4];
        hex_out[i * 2 + 1] = hx[digest[i] & 0xF];
    }
    hex_out[64] = '\0';
    return true;
#else
    (void)path; (void)hex_out; (void)hex_len;
    return false;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Jitter metrics — updated from the frame-arrival callback.
// All fields are std::atomic so the listener thread and the main thread can
// read them safely without a lock.
// ─────────────────────────────────────────────────────────────────────────────
struct FrameLog {
    uint64_t seq;
    uint64_t rx_timestamp_ns;
    uint64_t interval_ns;
};

struct Metrics {
    std::atomic<uint64_t> frames{0};
    std::atomic<uint64_t> sum_interval_ns{0};
    std::atomic<uint64_t> min_interval_ns{UINT64_MAX};
    std::atomic<uint64_t> max_interval_ns{0};
    // prev_rx_ns is written only by the listener thread — no atomic needed,
    // but use atomic for simplicity to avoid any UB if ever moved off-thread.
    std::atomic<uint64_t> prev_rx_ns{0};
    std::atomic<bool>     first_frame{true};
    
    FrameLog* frame_logs{nullptr};
    uint64_t  max_logs{0};
};

// Returns a callback suitable for set_frame_arrival_callback().
// Captures metrics by pointer (metrics lifetime must exceed the listener).
static std::function<void(uint64_t)> make_frame_callback(Metrics& m) {
    return [&m](uint64_t rx_ns) noexcept {
        uint64_t seq_idx = m.frames.fetch_add(1, std::memory_order_relaxed);

        bool expected_first = true;
        if (m.first_frame.compare_exchange_strong(expected_first, false,
                                                   std::memory_order_relaxed)) {
            // Record first timestamp; no interval yet.
            m.prev_rx_ns.store(rx_ns, std::memory_order_relaxed);
            if (seq_idx < m.max_logs && m.frame_logs) {
                m.frame_logs[seq_idx].seq = seq_idx + 1;
                m.frame_logs[seq_idx].rx_timestamp_ns = rx_ns;
                m.frame_logs[seq_idx].interval_ns = 0;
            }
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

        if (seq_idx < m.max_logs && m.frame_logs) {
            m.frame_logs[seq_idx].seq = seq_idx + 1;
            m.frame_logs[seq_idx].rx_timestamp_ns = rx_ns;
            m.frame_logs[seq_idx].interval_ns = interval;
        }
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Write CSV log
// ─────────────────────────────────────────────────────────────────────────────
static bool write_csv_log(const char* path, const FrameLog* logs, uint64_t count) noexcept {
    FILE* f = ::fopen(path, "w");
    if (!f) return false;
    ::fprintf(f, "seq,rx_timestamp_ns,interval_ns\n");
    for (uint64_t i = 0; i < count; ++i) {
        ::fprintf(f, "%llu,%llu,%llu\n", 
            (unsigned long long)logs[i].seq, 
            (unsigned long long)logs[i].rx_timestamp_ns, 
            (unsigned long long)logs[i].interval_ns);
    }
    ::fclose(f);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Write evidence JSON (no hash field — sidecar pattern avoids circularity)
// ─────────────────────────────────────────────────────────────────────────────
static bool write_evidence_json(
    const char*  path,
    const char*  started_at,
    const char*  ended_at,
    uint64_t     duration_ns,
    uint64_t     frames,
    uint64_t     min_ns,
    uint64_t     max_ns,
    uint64_t     mean_ns,
    uint64_t     jitter_ns,
    bool         connection_success,
    bool         wsl2,
    const char*  kernel_release,
    const char*  provenance_str,
    const char*  jitter_profile,     // "clean" | "has_multi_second_stalls"
    const char*  csv_filename,
    const char*  csv_sha256,
    bool         csv_truncated
) noexcept {
    FILE* f = ::fopen(path, "w");
    if (!f) return false;

    const char* env_class = wsl2 ? "WSL2_DISQUALIFIED" : "TIER0_CANDIDATE";
    const char* ev_class = (connection_success && !wsl2)
                           ? "LIVE_OPERATIONAL"
                           : (connection_success ? "EXTERNAL_LIVE_DISQUALIFIED_ENV" : "CONNECTION_FAILED");

    ::fprintf(f,
        "{\n"
        "  \"schema_version\": \"1\",\n"
        "  \"spec_ref\": \"S5-REQ-001\",\n"
        "  \"evidence_class\": \"%s\",\n"
        "  \"environment_class\": \"%s\",\n"
        "  \"source\": \"Binance\",\n"
        "  \"endpoint\": \"wss://stream.binance.com:9443/ws\",\n"
        "  \"provenance\": \"%s\",\n"
        "  \"started_at\": \"%s\",\n"
        "  \"ended_at\": \"%s\",\n"
        "  \"duration_ns\": %llu,\n"
        "  \"frames_received\": %llu,\n"
        "  \"timestamp_source\": \"CLOCK_MONOTONIC_RAW\",\n"
        "  \"min_inter_frame_ns\": %llu,\n"
        "  \"max_inter_frame_ns\": %llu,\n"
        "  \"mean_inter_frame_ns\": %llu,\n"
        "  \"jitter_ns\": %llu,\n"
        "  \"connection_success\": %s,\n"
        "  \"kernel_release\": \"%s\",\n"
        "  \"tier0_disqualified\": %s,\n"
        "  \"disqualification_reason\": \"%s\",\n"
        "  \"jitter_profile\": \"%s\",\n"
        "  \"jitter_profile_note\": \"PROVISIONAL sanity label, not a validated threshold\",\n"
        "  \"raw_frame_log\": \"%s\",\n"
        "  \"raw_frame_log_sha256\": \"%s\",\n"
        "  \"raw_frame_log_truncated\": %s\n"
        "}\n",
        ev_class,
        env_class,
        provenance_str,
        started_at,
        ended_at,
        (unsigned long long)duration_ns,
        (unsigned long long)frames,
        (unsigned long long)(min_ns == UINT64_MAX ? 0 : min_ns),
        (unsigned long long)max_ns,
        (unsigned long long)mean_ns,
        (unsigned long long)jitter_ns,
        connection_success ? "true" : "false",
        kernel_release,
        wsl2 ? "true" : "false",
        wsl2 ? "Host is WSL2: lacks isolcpus/nohz_full Tier-0 kernel parameters" : "none",
        jitter_profile,
        csv_filename,
        csv_sha256,
        csv_truncated ? "true" : "false"
    );
    ::fclose(f);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
#if defined(__linux__)
    ::signal(SIGPIPE, SIG_IGN);
#endif

    static constexpr int    kQualDurationSec = 60;
    static constexpr const char* kArtifactDir = "governance/proof_artifacts";
    static constexpr const char* kEvidencePath =
        "governance/proof_artifacts/s5_ac001_binance_evidence.json";
    static constexpr const char* kSidecarPath =
        "governance/proof_artifacts/s5_ac001_binance_evidence.json.sha256";
    static constexpr const char* kCsvPath =
        "governance/proof_artifacts/s5_ac001_binance_frames.csv";
    static constexpr const char* kCsvSidecarPath =
        "governance/proof_artifacts/s5_ac001_binance_frames.csv.sha256";

    int duration_sec = kQualDurationSec;
    if (argc >= 2) duration_sec = ::atoi(argv[1]);
    if (duration_sec <= 0) duration_sec = kQualDurationSec;

    // ── Environment detection ────────────────────────────────────────────────
    const bool wsl2_env = is_wsl2();
    char kernel_rel[128]{};
    get_kernel_release(kernel_rel, sizeof(kernel_rel));

    std::cout << "==================================================\n"
              << " EGBOS S5 Live Binance Qualification Harness\n"
              << " Spec: S5-REQ-001  Governance: MARKET-DATA-001\n"
              << "==================================================\n"
              << "Kernel        : " << kernel_rel << "\n"
              << "Environment   : " << (wsl2_env ? "WSL2 (Tier-0 DISQUALIFIED)" : "Bare-metal candidate") << "\n"
              << "Endpoint      : wss://stream.binance.com:9443/ws\n"
              << "Duration      : " << duration_sec << "s\n"
              << "Timestamp src : CLOCK_MONOTONIC_RAW (D4)\n"
              << "------------------------------------------------\n";

    if (wsl2_env) {
        std::cout << "[GOVERNANCE] WARNING: Running on WSL2.\n"
                  << "             Evidence will be classified EXTERNAL_LIVE_DISQUALIFIED_ENV.\n"
                  << "             This artifact MUST NOT be used to satisfy S5-AC-001.\n"
                  << "             S5-AC-001 requires a bare-metal Tier-0 host.\n"
                  << "             Proceeding for connectivity and software validation only.\n\n";
    }

    // ── SHM regions (stack allocation for qualification — no production SHM) ──
    static egbos::shm::MarketEventsRegion s_market_events{};
    static egbos::shm::GovernanceFlags    s_gov_flags{};

    // ── Metrics — populated via frame-arrival callback ───────────────────────
    static constexpr uint64_t kMaxCsvRows = 100000;
    auto frame_logs = std::make_unique<FrameLog[]>(kMaxCsvRows);

    Metrics metrics{};
    metrics.frame_logs = frame_logs.get();
    metrics.max_logs = kMaxCsvRows;

    // ── FeedIngestor ─────────────────────────────────────────────────────────
    FeedIngestor ingestor(&s_market_events, &s_gov_flags, /*tier0_fd=*/-1);

    // ── BinanceFeedListener configuration ────────────────────────────────────
    BinanceFeedConfig config;
    config.use_testnet    = false;                    // Production endpoint (D9)
    config.host           = "stream.binance.com";
    config.port           = 9443;
    config.stream_path    = "/ws/btcusdt@depth@100ms"; // Explicit raw-stream path
    config.auto_reconnect = false;

    BinanceFeedListener listener(ingestor, config);

    // ── Register frame-arrival callback ──────────────────────────────────────
    // This is the corrected instrumentation path: the callback is invoked from
    // within run_loop() immediately after process_payload(), using the same
    // CLOCK_MONOTONIC_RAW timestamp already captured at frame receive.
    listener.set_frame_arrival_callback(make_frame_callback(metrics));

    std::cout << "Provenance    : " << listener.provenance_str() << "\n\n"
              << "Endpoint : wss://stream.binance.com:9443/ws/btcusdt@depth@100ms\n";

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

    // PROVISIONAL sanity flag based on a single 120s sample; not a validated
    // pass/fail criterion — see project notes. Does NOT affect exit code.
    // 1,000,000,000 ns == 1000 ms: any single inter-frame gap above this
    // indicates a multi-second stall rather than ordinary scheduling jitter.
    static constexpr uint64_t kMultiSecondStallThresholdNs = 1'000'000'000ULL;
    const char* jitter_profile = (max_ns > kMultiSecondStallThresholdNs)
                                 ? "has_multi_second_stalls"
                                 : "clean";

    std::cout << "\n------------------------------------------------\n"
              << "Results:\n"
              << "  Duration    : " << duration_ns / 1'000'000ULL << " ms\n"
              << "  Frames recv : " << n_frames << "\n"
              << "  Conn success: " << (connection_success ? "YES" : "NO") << "\n"
              << "  Min inter-frame : " << (min_ns == UINT64_MAX ? 0 : min_ns) << " ns\n"
              << "  Max inter-frame : " << max_ns << " ns\n"
              << "  Mean inter-frame: " << mean_ns << " ns\n"
              << "  Jitter (p2p)    : " << jitter_ns << " ns\n"
              << "  Evidence class  : "
              << (connection_success && !wsl2_env ? "LIVE_OPERATIONAL"
                  : connection_success ? "EXTERNAL_LIVE_DISQUALIFIED_ENV"
                  : "CONNECTION_FAILED")
              << "\n"
              << "------------------------------------------------\n\n";

    // ── Write CSV artifact ───────────────────────────────────────────────────
    ::system(("mkdir -p " + std::string(kArtifactDir)).c_str());
    
    uint64_t csv_rows = (n_frames > metrics.max_logs) ? metrics.max_logs : n_frames;
    bool csv_truncated = (n_frames > metrics.max_logs);
    if (!write_csv_log(kCsvPath, frame_logs.get(), csv_rows)) {
        std::cerr << "ERROR: Failed to write CSV log: " << kCsvPath << "\n";
        return 1;
    }
    
    char hex_csv_sha256[65]{};
    if (!sha256_file(kCsvPath, hex_csv_sha256, sizeof(hex_csv_sha256))) {
        std::cerr << "ERROR: SHA256 computation failed for: " << kCsvPath << "\n";
        return 1;
    }
    
    FILE* csv_sidecar = ::fopen(kCsvSidecarPath, "w");
    if (!csv_sidecar) {
        std::cerr << "ERROR: Failed to write CSV SHA256 sidecar: " << kCsvSidecarPath << "\n";
        return 1;
    }
    ::fprintf(csv_sidecar, "%s  s5_ac001_binance_frames.csv\n", hex_csv_sha256);
    ::fclose(csv_sidecar);

    // ── Write evidence artifact ────────────────────────────────────────────────
    const bool wrote_json = write_evidence_json(
        kEvidencePath,
        started_at, ended_at, duration_ns,
        n_frames,
        min_ns, max_ns, mean_ns, jitter_ns,
        connection_success,
        wsl2_env, kernel_rel,
        listener.provenance_str(),
        jitter_profile,
        "s5_ac001_binance_frames.csv",
        hex_csv_sha256,
        csv_truncated
    );

    if (!wrote_json) {
        std::cerr << "ERROR: Failed to write evidence artifact: " << kEvidencePath << "\n";
        return 1;
    }
    std::cout << "Evidence artifact written: " << kEvidencePath << "\n";

    // ── SHA256 sidecar ────────────────────────────────────────────────────────
    char hex_sha256[65]{};
    if (!sha256_file(kEvidencePath, hex_sha256, sizeof(hex_sha256))) {
        std::cerr << "ERROR: SHA256 computation failed for: " << kEvidencePath << "\n";
        return 1;
    }

    FILE* sidecar = ::fopen(kSidecarPath, "w");
    if (!sidecar) {
        std::cerr << "ERROR: Failed to write SHA256 sidecar: " << kSidecarPath << "\n";
        return 1;
    }
    ::fprintf(sidecar, "%s  s5_ac001_binance_evidence.json\n", hex_sha256);
    ::fclose(sidecar);

    std::cout << "SHA256 sidecar written : " << kSidecarPath << "\n"
              << "SHA256                 : " << hex_sha256 << "\n\n";

    if (wsl2_env) {
        std::cout << "[GOVERNANCE] This artifact is DISQUALIFIED for S5-AC-001 evidence.\n"
                  << "             Reason: WSL2 environment lacks Tier-0 kernel isolation.\n"
                  << "             Re-run on a bare-metal Tier-0 host to produce qualifying evidence.\n";
        return 2;
    }

    if (!connection_success) {
        std::cout << "[GOVERNANCE] Connection FAILED. Evidence is not LIVE_OPERATIONAL.\n"
                  << "             Check network access to stream.binance.com:9443.\n";
        return 3;
    }

    if (n_frames == 0) {
        std::cout << "[GOVERNANCE] Connection succeeded but ZERO frames received.\n"
                  << "             Jitter measurement is INVALID. S5-AC-001 is NOT satisfied.\n"
                  << "             Investigate stream subscription path and receive loop.\n";
        return 4;
    }

    std::cout << "[GOVERNANCE] Evidence class: LIVE_OPERATIONAL\n"
              << "             This artifact may be used for S5-AC-001 qualification\n"
              << "             subject to all other governance prerequisites.\n";
    return 0;
}
