// scripts/p7_smoke_harness.cpp
// EGBOS P7 Staging Smoke Harness — L5 SPSC Latency Measurement
//
// Specification : EGBOS Master Blueprint §B1.4.2 — P7 Gate
//                 "60s simulator smoke, P99 within 20% baseline"
// Blueprint ref : Line 1130 — P7 | Staging Smoke | main merges | Phase 4
// Baseline ref  : governance/proof_artifacts/timing_constants.json
//                 spsc_push_pop_p99_ns (dev-workstation-001 baseline)
//
// What this measures:
//   The round-trip latency of one ExecCommand push (producer) + pop (consumer)
//   through the egbos_exec_commands SPSC ring (L3→L5). This is the core
//   timing claim for P7: the execution router must drain commands within
//   latency bounds established in timing_constants.json.
//
// Govcheck compliance:
//   GC-E-010: no malloc/new/delete on hot path (stack-only latency array)
//   GC-E-011: CLOCK_MONOTONIC only — CLOCK_REALTIME is forbidden on hot path
//   GC-E-042: no business logic — pure push/pop measurement
//
// Output: JSON artifact to governance/proof_artifacts/p7_staging_smoke_results.json

#include "../engine/core/spsc_ring.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <fstream>
#include <ctime>

// ── ExecCommand slot — matches layout in execution_router.h ─────────────────
// Spec §6.2: egbos_exec_commands SPSC ring, 4096 slots × 64B
// session_id carries the push timestamp (ns) for RTT measurement.
struct alignas(64) ExecCommand {
    uint64_t session_id;     // push timestamp (ns) for RTT
    uint64_t msg_seq_num;
    int64_t  price;          // fixed-point, PRICE_SCALE=100000
    int64_t  quantity;
    uint8_t  r_applied;
    uint8_t  side;
    uint8_t  _reserved[30]; // pad to 64 bytes: 8+8+8+8+1+1+30 = 64
};
static_assert(sizeof(ExecCommand) == 64, "ExecCommand must be exactly 64 bytes");

// ── Configuration ─────────────────────────────────────────────────────────────

static constexpr int    kDurationSeconds = 60;
static constexpr size_t kMaxSamples      = 10'000'000; // pre-allocated, no heap on hot path
static constexpr size_t kRingCapacity    = 4096;       // §6.2: egbos_exec_commands 4096 slots

// ── Inline timing helper (CLOCK_MONOTONIC, not CLOCK_REALTIME — GC-E-011) ────

static inline int64_t now_ns() noexcept {
    struct timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

// ── SPSC ring instance ────────────────────────────────────────────────────────

using ExecSlot = ExecCommand;
static egbos::core::SpscRing<ExecSlot, kRingCapacity> g_ring;

// ── Shared latency buffer (pre-allocated before hot path) ─────────────────────

static std::vector<int64_t> g_latencies;

// ── Producer thread ───────────────────────────────────────────────────────────
// Pushes synthetic ExecCommands for kDurationSeconds.
// Records push timestamp in slot's session_id field so consumer can measure RTT.

static std::atomic<bool>  g_stop{false};
static std::atomic<bool>  g_producer_ready{false};

static void producer_fn() {
    g_producer_ready.store(true, std::memory_order_release);

    ExecSlot cmd{};
    cmd.msg_seq_num = 1;
    cmd.r_applied   = 1;
    cmd.side        = 0;  // buy
    cmd.price       = 10000000LL;  // $100.00 at PRICE_SCALE=100000
    cmd.quantity    = 1000000LL;   // 10 lots

    while (!g_stop.load(std::memory_order_acquire)) {
        cmd.session_id = static_cast<uint64_t>(now_ns()); // timestamp encoded in slot
        ++cmd.msg_seq_num;
        g_ring.push(cmd);
    }
}

// ── Consumer thread ───────────────────────────────────────────────────────────
// Pops ExecCommands, computes RTT = now - slot.session_id, records into g_latencies.

static void consumer_fn() {
    ExecSlot out{};
    size_t count = 0;
    size_t iters = 0;

    while (!g_stop.load(std::memory_order_acquire)) {
        if (g_ring.pop(out)) {
            const int64_t t_pop  = now_ns();
            const int64_t t_push = static_cast<int64_t>(out.session_id);
            const int64_t rtt    = t_pop - t_push;
            
            // Sample every 128th push to ensure the 10M pre-allocated array 
            // covers the full 60 seconds without exhausting or allocating.
            if (rtt > 0 && (++iters & 127) == 0 && count < g_latencies.size()) {
                g_latencies[count++] = rtt;
            }
        }
    }

    // Drain remaining items after stop signal
    while (g_ring.pop(out)) {
        const int64_t t_pop  = now_ns();
        const int64_t t_push = static_cast<int64_t>(out.session_id);
        const int64_t rtt    = t_pop - t_push;
        if (rtt > 0 && (++iters & 127) == 0 && count < g_latencies.size()) {
            g_latencies[count++] = rtt;
        }
    }

    g_latencies.resize(count);
}

// ── Percentile helper ─────────────────────────────────────────────────────────

static int64_t percentile(std::vector<int64_t>& v, double pct) {
    if (v.empty()) return 0;
    const size_t idx = static_cast<size_t>(pct / 100.0 * (v.size() - 1));
    return v[std::min(idx, v.size() - 1)];
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    const char* output_path = (argc >= 2) ? argv[1]
        : "governance/proof_artifacts/p7_staging_smoke_results.json";
    const char* timing_path = (argc >= 3) ? argv[2]
        : "governance/proof_artifacts/timing_constants.json";

    // ── Load baseline from timing_constants.json ──────────────────────────────
    int64_t baseline_p99_ns = 0;
    {
        FILE* f = ::fopen(timing_path, "r");
        if (!f) {
            ::fprintf(stderr, "ERROR: Cannot open %s\n", timing_path);
            return 1;
        }
        char buf[4096]{};
        ::fread(buf, 1, sizeof(buf) - 1, f);
        ::fclose(f);

        // Simple string scan for spsc_push_pop_p99_ns value
        const char* key = "\"spsc_push_pop_p99_ns\"";
        const char* pos = ::strstr(buf, key);
        if (!pos) {
            ::fprintf(stderr, "ERROR: spsc_push_pop_p99_ns not found in %s\n", timing_path);
            return 1;
        }
        const char* colon = ::strchr(pos, ':');
        if (!colon) {
            ::fprintf(stderr, "ERROR: malformed timing_constants.json\n");
            return 1;
        }
        baseline_p99_ns = ::strtoll(colon + 1, nullptr, 10);
    }

    if (baseline_p99_ns <= 0) {
        ::fprintf(stderr, "ERROR: baseline_p99_ns is zero or negative (%lld)\n",
                  (long long)baseline_p99_ns);
        return 1;
    }

    ::printf("=== EGBOS P7 Staging Smoke Gate ===\n");
    ::printf("Duration        : %d seconds\n", kDurationSeconds);
    ::printf("Baseline P99    : %lld ns (from timing_constants.json)\n",
             (long long)baseline_p99_ns);
    ::printf("Pass threshold  : %lld ns (baseline × 1.20)\n",
             (long long)(baseline_p99_ns * 12 / 10));

    // ── Pre-allocate latency buffer (no heap on hot path) ─────────────────────
    g_latencies.reserve(kMaxSamples);
    g_latencies.resize(kMaxSamples, 0);

    // ── Start producer; wait for ready signal ────────────────────────────────
    std::thread producer(producer_fn);
    while (!g_producer_ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // ── Start consumer ────────────────────────────────────────────────────────
    std::thread consumer(consumer_fn);

    // ── Run for 60 seconds ────────────────────────────────────────────────────
    const auto start = std::chrono::steady_clock::now();
    ::printf("Running smoke test for %ds...\n", kDurationSeconds);
    std::this_thread::sleep_for(std::chrono::seconds(kDurationSeconds));
    g_stop.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    const auto end       = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(end - start).count();

    if (g_latencies.empty()) {
        ::fprintf(stderr, "ERROR: No latency samples collected.\n");
        return 1;
    }

    // ── Compute statistics ────────────────────────────────────────────────────
    std::sort(g_latencies.begin(), g_latencies.end());

    const int64_t p50  = percentile(g_latencies, 50.0);
    const int64_t p95  = percentile(g_latencies, 95.0);
    const int64_t p99  = percentile(g_latencies, 99.0);
    const int64_t pmax = g_latencies.back();

    const double regression_pct = ((double)(p99 - baseline_p99_ns) / (double)baseline_p99_ns) * 100.0;
    const bool   pass           = (p99 <= baseline_p99_ns * 12 / 10);  // ≤ 20% regression

    ::printf("\n=== Results ===\n");
    ::printf("Samples         : %zu\n",          g_latencies.size());
    ::printf("Duration (s)    : %.2f\n",         elapsed);
    ::printf("P50  latency    : %lld ns\n",      (long long)p50);
    ::printf("P95  latency    : %lld ns\n",      (long long)p95);
    ::printf("P99  latency    : %lld ns\n",      (long long)p99);
    ::printf("Max  latency    : %lld ns\n",      (long long)pmax);
    ::printf("Baseline P99    : %lld ns\n",      (long long)baseline_p99_ns);
    ::printf("Regression      : %.2f%%\n",       regression_pct);
    ::printf("Gate            : %s\n",           pass ? "PASS" : "FAIL");

    // ── Write JSON artifact ───────────────────────────────────────────────────
    {
        FILE* f = ::fopen(output_path, "w");
        if (!f) {
            ::fprintf(stderr, "ERROR: Cannot write to %s\n", output_path);
            return 1;
        }
        ::fprintf(f,
            "{\n"
            "  \"schema_version\": 1,\n"
            "  \"status\": \"%s\",\n"
            "  \"note\": \"P7 staging smoke: SPSC push/pop RTT measured on dev-workstation-001 for %ds.\",\n"
            "  \"measurements\": {\n"
            "    \"execution_duration_s\": %.2f,\n"
            "    \"sample_count\": %zu,\n"
            "    \"p50_latency_ns\": %lld,\n"
            "    \"p95_latency_ns\": %lld,\n"
            "    \"p99_latency_ns\": %lld,\n"
            "    \"max_latency_ns\": %lld,\n"
            "    \"baseline_p99_ns\": %lld,\n"
            "    \"regression_pct\": %.2f\n"
            "  }\n"
            "}\n",
            pass ? "PASS" : "FAIL",
            kDurationSeconds,
            elapsed,
            g_latencies.size(),
            (long long)p50,
            (long long)p95,
            (long long)p99,
            (long long)pmax,
            (long long)baseline_p99_ns,
            regression_pct
        );
        ::fclose(f);
    }

    ::printf("Artifact written to %s\n", output_path);

    if (!pass) {
        ::fprintf(stderr, "P7 Gate: FAIL — P99 %lld ns exceeds baseline %lld ns + 20%%\n",
                  (long long)p99, (long long)baseline_p99_ns);
        return 1;
    }

    ::printf("P7 Gate: PASS\n");
    return 0;
}
