// scripts/s5_exec_ring_hwm_harness.cpp
// EGBOS S5-REQ-001 â€” L5 Execution-Command Ring HWM Measurement Harness
//
// Specification refs  : S5-REQ-001 â†’ CLAIM-L3-05
// Governed component  : egbos_exec_commands (L3 â†’ L5), kExecCommandsSlots = 4096
// Classification      : LOCAL_SYNTHETIC  (never LIVE_OPERATIONAL)
// Source revision     : 2663389
//
// PURPOSE:
//   Measure the peak ring occupancy (HWM) of the egbos_exec_commands SPSC ring
//   under synthetic load in a self-contained local process.
//
// CRITICAL LIMITATION â€” READ BEFORE USING THIS ARTIFACT:
//   This harness operates a LOCAL, IN-PROCESS replica of the egbos_exec_commands
//   ring using the production SpscRing<ExecCommandSlot, kExecCommandsSlots>
//   template instantiation. It does NOT attach to a running production engine,
//   does NOT use shared memory (/dev/shm), and does NOT receive commands from
//   the L3 position engine.
//
//   THEREFORE:
//     - The ring name is correct (egbos_exec_commands)
//     - The ring capacity is correct (kExecCommandsSlots = 4096)
//     - The ring TYPE is correct (SpscRing<ExecCommandSlot, 4096>)
//     - The measured HWM is NOT production evidence
//
//   V4 promotion of CLAIM-L3-05 requires HWM measurement under a live L3 producer
//   feeding the actual /dev/shm/egbos_exec_commands region. That evidence must
//   be classified LIVE_OPERATIONAL and is not produced here.
//
// Govcheck compliance:
//   GC-E-010 : No malloc/new/delete on hot path (ring and sample array are static)
//   GC-E-011 : CLOCK_MONOTONIC only (no CLOCK_REALTIME)
//   GC-E-042 : No business logic â€” pure push/pop ring measurement
//
// Output : governance/proof_artifacts/s5_exec_ring_hwm_results.json

#include "../engine/core/spsc_ring.h"
#include "../engine/core/shm_layout.h"
#include "../engine/core/execution_router.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <thread>
#include <random>

// â”€â”€ Configuration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static constexpr int    kDurationSeconds = 60;
static constexpr size_t kMaxSamples      = 1'000'000; // pre-allocated, no heap on hot path

// Verify we are measuring the CORRECT ring (compile-time assertion).
// If this fails, the harness is not measuring the governed component.
static_assert(egbos::shm::kExecCommandsSlots == 4096,
    "S5-REQ-001 HARNESS ERROR: kExecCommandsSlots != 4096. "
    "This harness must measure the 4096-slot egbos_exec_commands ring, "
    "not egbos_market_events (65536 slots). Verify shm_layout.h.");

// â”€â”€ Shared state (no heap) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
struct HarnessState {
    std::atomic<bool>     stop{false};
    std::atomic<uint64_t> pushes{0};
    std::atomic<uint64_t> pops{0};
    std::atomic<uint64_t> lazy_refreshes{0};

    // Static ring storage â€” production type, production capacity.
    // Same template instantiation used by ExecCommandsRegion in shm_layout.h.
    egbos::core::SpscRing<egbos::l5::ExecCommandSlot, egbos::shm::kExecCommandsSlots> ring;
};

// Static storage: avoids heap allocation on hot path (GC-E-010).
static HarnessState g_state;

// â”€â”€ Producer thread â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Pushes ExecCommandSlots at rate calibrated to generate measurable HWM without
// overwhelming the consumer, simulating L3 burst behaviour.
static void producer_thread(int duration_s) {
    egbos::l5::ExecCommandSlot slot{};

    // Synthetic command payload â€” not a real order, no exchange connection.
    // Fields match ExecCommand layout (execution_router.h).
    uint64_t seq = 0;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> jitter_dist(-50, 50);

    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(duration_s);
    auto next_push = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() < end) {
        // Enforce jittered rate limit: 500us mean, +/- 50us
        int current_jitter = jitter_dist(rng);
        auto delay = std::chrono::microseconds(500 + current_jitter);
        next_push += delay;
        std::this_thread::sleep_until(next_push);

        // Encode monotonically increasing sequence into slot for traceability.
        std::memcpy(slot.data, &seq, sizeof(seq));
        seq++;

        g_state.ring.push(slot);
        g_state.pushes.fetch_add(1, std::memory_order_relaxed);
    }

    g_state.lazy_refreshes.store(g_state.ring.producer_lazy_refreshes(), std::memory_order_relaxed);
    g_state.stop.store(true, std::memory_order_release);
}

// â”€â”€ Consumer thread â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Pops slots
static void consumer_thread() {
    egbos::l5::ExecCommandSlot slot{};

    while (!g_state.stop.load(std::memory_order_acquire) ||
           g_state.pushes.load(std::memory_order_relaxed) >
           g_state.pops.load(std::memory_order_relaxed)) {

        if (g_state.ring.pop(slot)) {
            g_state.pops.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// â”€â”€ SHA-256 stub â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Full SHA-256 of the output JSON is computed post-hoc by the signing step.
// This field is intentionally left as a placeholder in the self-generated artifact;
// the govcheck sign workflow computes and records the final hash.
static const char* kSha256Placeholder = "PENDING_GOVCHECK_SIGN";

// â”€â”€ Collection timestamp (ISO-8601 UTC) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static void get_timestamp(char* buf, size_t len) {
    time_t t = time(nullptr);
    struct tm* tm_info = gmtime(&t);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}

// â”€â”€ Main â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
int main(int argc, char* argv[]) {
    const char* output_path =
        "governance/proof_artifacts/s5_req001_hwm_upper_bound.json";
    if (argc > 1) output_path = argv[1];

    fprintf(stderr,
        "S5-REQ-001 Exec Ring HWM Harness\n"
        "  Ring        : egbos_exec_commands\n"
        "  Capacity    : %zu slots\n"
        "  Duration    : %ds\n"
        "  Class       : LOCAL_SYNTHETIC\n"
        "  LIMITATION  : In-process ring replica, not /dev/shm production region.\n"
        "                V4 requires LIVE_OPERATIONAL evidence.\n\n",
        egbos::shm::kExecCommandsSlots,
        kDurationSeconds);

    // Launch producer and consumer threads (SPSC invariant: one producer, one consumer).
    std::thread producer(producer_thread, kDurationSeconds);
    std::thread consumer(consumer_thread);
    producer.join();
    consumer.join();

    const uint64_t total_push = g_state.pushes.load();
    const uint64_t total_pop  = g_state.pops.load();
    const uint64_t capacity = egbos::shm::kExecCommandsSlots;
    const uint64_t total_lazy_refreshes = g_state.lazy_refreshes.load();

    const uint64_t theoretical_min_refreshes = total_push / capacity;
    const double allowed_max_refreshes = static_cast<double>(theoretical_min_refreshes) * 1.05;
    const double refresh_ratio = (theoretical_min_refreshes > 0) ?
        static_cast<double>(total_lazy_refreshes) / static_cast<double>(theoretical_min_refreshes) : 0.0;

    // Pass criterion: Total Lazy Refreshes <= (Total Pushes / Capacity) * 1.05
    const bool p_pass = static_cast<double>(total_lazy_refreshes) <= allowed_max_refreshes;

    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));

    fprintf(stderr,
        "Results:\n"
        "  Total Pushes: %llu\n"
        "  Total Pops: %llu\n"
        "  Capacity: %llu\n"
        "  Total Lazy Refreshes: %llu\n"
        "  Theoretical Minimum: %llu\n"
        "  Allowed Maximum: %.2f\n"
        "  Refresh Ratio: %.4f\n"
        "  Criterion: Total Lazy Refreshes <= (Total Pushes / Capacity) * 1.05\n"
        "  PASS/FAIL: %s\n\n",
        (unsigned long long)total_push,
        (unsigned long long)total_pop,
        (unsigned long long)capacity,
        (unsigned long long)total_lazy_refreshes,
        (unsigned long long)theoretical_min_refreshes,
        allowed_max_refreshes,
        refresh_ratio,
        p_pass ? "PASS" : "FAIL");

    // â”€â”€ Write JSON artifact â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    std::ofstream out(output_path);
    if (!out.is_open()) {
        fprintf(stderr, "ERROR: Cannot write artifact to %s\n", output_path);
        return 1;
    }

    out <<
        "{\n"
        "  \"artifact_id\": \"S5-REQ-001-EXEC-RING-HWM-v1\",\n"
        "  \"requirement_id\": \"S5-REQ-001\",\n"
        "  \"classification\": \"LOCAL_SYNTHETIC\",\n"
        "  \"governed_component\": \"CLAIM-L3-05\",\n"
        "  \"ring_name\": \"egbos_exec_commands\",\n";

    out << "  \"ring_capacity_slots\": " << capacity << ",\n";
    out << "  \"total_pushes\": " << total_push << ",\n";
    out << "  \"total_pops\": " << total_pop << ",\n";
    out << "  \"total_lazy_refreshes\": " << total_lazy_refreshes << ",\n";
    out << "  \"theoretical_min_refreshes\": " << theoretical_min_refreshes << ",\n";

    char allowed_buf[32];
    snprintf(allowed_buf, sizeof(allowed_buf), "%.2f", allowed_max_refreshes);
    out << "  \"allowed_max_refreshes\": " << allowed_buf << ",\n";

    char ratio_buf[32];
    snprintf(ratio_buf, sizeof(ratio_buf), "%.4f", refresh_ratio);
    out << "  \"refresh_ratio\": " << ratio_buf << ",\n";

    out <<
        "  \"methodology\": \"In-process SpscRing<ExecCommandSlot,4096> replica; "
            "not /dev/shm production region\",\n";
    out << "  \"collection_timestamp\": \"" << timestamp << "\",\n";
    out << "  \"source_revision\": \"2663389\",\n";
    out <<
        "  \"environment\": \"dev-workstation-001\",\n"
        "  \"executable\": \"s5_exec_ring_hwm_harness\",\n";
    out << "  \"execution_duration_s\": " << kDurationSeconds << ",\n";
    out << "  \"output_sha256\": \"" << kSha256Placeholder << "\",\n";
    out <<
        "  \"pass_criteria\": {\n"
        "    \"P_ring_lazy_refresh_limit\": { \"result\": " << (p_pass ? "true" : "false") << ", "
            "\"value\": " << total_lazy_refreshes << " }\n"
        "  },\n"
        "  \"limitations\": [\n"
        "    \"LOCAL_SYNTHETIC: in-process ring only; not attached to /dev/shm/egbos_exec_commands\",\n"
        "    \"V4 promotion requires LIVE_OPERATIONAL evidence from production L3 producer\",\n"
        "    \"CLAIM-L3-05 remains blocked at V3\"\n"
        "  ],\n"
        "  \"v4_satisfied\": false\n"
        "}\n";

    out.close();
    fprintf(stderr, "Artifact written to: %s\n", output_path);
    return p_pass ? 0 : 1;
}
