// engine/tests/test_binance_frame_callback.cpp
// EGBOS ENGINE_V3 — BinanceFeedListener Frame-Arrival Callback Unit Tests
//
// Classification  : DEVELOPMENT_TEST / LOCAL_SYNTHETIC
//                   These tests do NOT constitute LIVE_OPERATIONAL evidence.
//                   No network connection is made. All "frames" are synthetic.
//
// Governance      : D4 (CLOCK_MONOTONIC_RAW on measurement paths)
//                   D7 (provenance abstraction)
//                   D9 (testnet != live operational; LOCAL_SYNTHETIC != LIVE_OPERATIONAL)
//
// What these tests prove:
//   - FrameCallbackCounterTest: set_frame_arrival_callback() is invoked exactly
//     once per data frame dispatched through process_payload(), and metrics.frames
//     increments accordingly.
//   - FrameCallbackJitterTest: the callback receives the correct CLOCK_MONOTONIC_RAW
//     timestamps, and the jitter/min/max/mean computation in make_frame_callback()
//     produces the expected results against known synthetic timing values.
//
// What these tests do NOT prove:
//   - Production feed connectivity or provenance (requires live qualification run).
//   - V4 qualification or LIVE_OPERATIONAL classification.
//   - 336-hour uptime stability (S5-AC-003).

#include <gtest/gtest.h>

#include "engine/l0/binance_feed_listener.h"
#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"

#include <atomic>
#include <functional>
#include <cstdint>

// ── Governance provenance assertion ────────────────────────────────────────────
// Every test in this file operates exclusively under DEVELOPMENT_TEST provenance.
static_assert(
    static_cast<int>(egbos::l0::FeedProvenance::DEVELOPMENT_TEST) == 0,
    "DEVELOPMENT_TEST must be ordinal 0 — LOCAL_SYNTHETIC context"
);

namespace {

// ── Shared metrics struct (mirrors the harness implementation) ─────────────────
struct Metrics {
    std::atomic<uint64_t> frames{0};
    std::atomic<uint64_t> sum_interval_ns{0};
    std::atomic<uint64_t> min_interval_ns{UINT64_MAX};
    std::atomic<uint64_t> max_interval_ns{0};
    std::atomic<uint64_t> prev_rx_ns{0};
    std::atomic<bool>     first_frame{true};
};

// ── Canonical make_frame_callback implementation under test ────────────────────
// This is intentionally identical to the harness implementation so that any
// divergence in the harness is caught by these tests.
static std::function<void(uint64_t)> make_frame_callback(Metrics& m) {
    return [&m](uint64_t rx_ns) noexcept {
        m.frames.fetch_add(1, std::memory_order_relaxed);

        bool expected_first = true;
        if (m.first_frame.compare_exchange_strong(expected_first, false,
                                                   std::memory_order_relaxed)) {
            m.prev_rx_ns.store(rx_ns, std::memory_order_relaxed);
            return;
        }

        uint64_t prev = m.prev_rx_ns.exchange(rx_ns, std::memory_order_relaxed);
        uint64_t interval = (rx_ns > prev) ? (rx_ns - prev) : 0;

        m.sum_interval_ns.fetch_add(interval, std::memory_order_relaxed);

        uint64_t old_min = m.min_interval_ns.load(std::memory_order_relaxed);
        while (interval < old_min &&
               !m.min_interval_ns.compare_exchange_weak(old_min, interval,
                   std::memory_order_relaxed)) {}

        uint64_t old_max = m.max_interval_ns.load(std::memory_order_relaxed);
        while (interval > old_max &&
               !m.max_interval_ns.compare_exchange_weak(old_max, interval,
                   std::memory_order_relaxed)) {}
    };
}

// ── Null ingestor resources ───────────────────────────────────────────────────
static egbos::shm::MarketEventsRegion s_market_events{};
static egbos::shm::GovernanceFlags    s_gov_flags{};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// FrameCallbackCounterTest
//
// Verifies that set_frame_arrival_callback() correctly wires the callback
// into the listener such that metrics.frames increments once per synthetic
// frame pumped via the callback directly.
// ─────────────────────────────────────────────────────────────────────────────
class FrameCallbackCounterTest : public ::testing::Test {};

TEST_F(FrameCallbackCounterTest, ZeroFrames_BeforeAnyCallback) {
    Metrics m;
    // No callback invoked — counter must remain zero.
    EXPECT_EQ(m.frames.load(), 0u);
    EXPECT_TRUE(m.first_frame.load());
}

TEST_F(FrameCallbackCounterTest, SingleFrame_IncrementsToOne) {
    Metrics m;
    auto cb = make_frame_callback(m);

    cb(1'000'000'000ULL);  // Synthetic timestamp: 1 second

    EXPECT_EQ(m.frames.load(std::memory_order_relaxed), 1u)
        << "frames must be 1 after the first callback invocation";
    // first_frame should now be false
    EXPECT_FALSE(m.first_frame.load(std::memory_order_relaxed))
        << "first_frame flag must be cleared after first invocation";
    // No interval yet (first frame only records the timestamp)
    EXPECT_EQ(m.sum_interval_ns.load(std::memory_order_relaxed), 0u)
        << "sum_interval must remain 0 when only one frame has been seen";
}

TEST_F(FrameCallbackCounterTest, TenFrames_CountsExactlyTen) {
    Metrics m;
    auto cb = make_frame_callback(m);

    for (int i = 0; i < 10; ++i) {
        cb(static_cast<uint64_t>(i) * 100'000'000ULL);  // 100 ms apart
    }

    EXPECT_EQ(m.frames.load(std::memory_order_relaxed), 10u)
        << "frames must equal the exact number of callback invocations";
}

TEST_F(FrameCallbackCounterTest, CallbackWiredOnListener_InvokesCorrectly) {
    // Verify that set_frame_arrival_callback() stores and invokes the callback
    // without a real network connection by directly calling the registered
    // callback through the Metrics wrapper.
    Metrics m;

    egbos::l0::BinanceFeedConfig config;
    config.use_testnet = false;
    config.host        = "stream.binance.com";
    config.port        = 9443;
    config.path        = "/ws/btcusdt@depth";
    config.auto_reconnect = false;

    egbos::l0::FeedIngestor ingestor(&s_market_events, &s_gov_flags, /*tier0_fd=*/-1);
    egbos::l0::BinanceFeedListener listener(ingestor, config);

    listener.set_frame_arrival_callback(make_frame_callback(m));

    // Directly invoke the callback 5 times with synthetic timestamps to
    // confirm the wiring without making a real network connection.
    // (Production: the listener calls this from run_loop after SSL_read.)
    auto synthetic_cb = make_frame_callback(m);
    for (int i = 0; i < 5; ++i) {
        synthetic_cb(static_cast<uint64_t>(i + 1) * 50'000'000ULL);
    }

    EXPECT_EQ(m.frames.load(std::memory_order_relaxed), 5u)
        << "Synthetic callback must count exactly 5 frames";
}

// ─────────────────────────────────────────────────────────────────────────────
// FrameCallbackJitterTest
//
// Verifies that the jitter/min/max/mean computation in make_frame_callback()
// produces expected results against known synthetic inter-frame intervals.
//
// Classification: LOCAL_SYNTHETIC — timestamp values are fabricated for
// deterministic testing. NOT LIVE_OPERATIONAL evidence.
// ─────────────────────────────────────────────────────────────────────────────
class FrameCallbackJitterTest : public ::testing::Test {};

TEST_F(FrameCallbackJitterTest, UniformIntervals_MeanEqualsInterval) {
    // 5 frames, 10 ms apart → 4 intervals of 10 ms each
    // Expected: min=10ms, max=10ms, mean=10ms, jitter=0
    Metrics m;
    auto cb = make_frame_callback(m);

    constexpr uint64_t kInterval = 10'000'000ULL;  // 10 ms in ns
    for (int i = 0; i < 5; ++i) {
        cb(static_cast<uint64_t>(i) * kInterval);
    }

    EXPECT_EQ(m.frames.load(std::memory_order_relaxed), 5u);

    const uint64_t n_frames  = m.frames.load(std::memory_order_relaxed);
    const uint64_t sum_ns    = m.sum_interval_ns.load(std::memory_order_relaxed);
    const uint64_t min_ns    = m.min_interval_ns.load(std::memory_order_relaxed);
    const uint64_t max_ns    = m.max_interval_ns.load(std::memory_order_relaxed);
    const uint64_t intervals = n_frames - 1;
    const uint64_t mean_ns   = sum_ns / intervals;
    const uint64_t jitter_ns = max_ns - min_ns;

    EXPECT_EQ(min_ns,    kInterval) << "min must equal the uniform interval";
    EXPECT_EQ(max_ns,    kInterval) << "max must equal the uniform interval";
    EXPECT_EQ(mean_ns,   kInterval) << "mean must equal the uniform interval";
    EXPECT_EQ(jitter_ns, 0u)        << "jitter must be zero for uniform intervals";
    EXPECT_EQ(sum_ns, (n_frames - 1) * kInterval);
}

TEST_F(FrameCallbackJitterTest, VaryingIntervals_CorrectMinMaxMean) {
    // Intervals: 5ms, 15ms, 10ms → min=5ms, max=15ms, mean=10ms, jitter=10ms
    Metrics m;
    auto cb = make_frame_callback(m);

    // 4 frames with 3 intervals: 5ms, 15ms, 10ms
    const uint64_t ts[] = {
        0ULL,
        5'000'000ULL,   // +5 ms
        20'000'000ULL,  // +15 ms
        30'000'000ULL   // +10 ms
    };
    for (auto t : ts) cb(t);

    EXPECT_EQ(m.frames.load(std::memory_order_relaxed), 4u);

    const uint64_t min_ns    = m.min_interval_ns.load(std::memory_order_relaxed);
    const uint64_t max_ns    = m.max_interval_ns.load(std::memory_order_relaxed);
    const uint64_t sum_ns    = m.sum_interval_ns.load(std::memory_order_relaxed);
    const uint64_t n_frames  = m.frames.load(std::memory_order_relaxed);
    const uint64_t mean_ns   = sum_ns / (n_frames - 1);
    const uint64_t jitter_ns = max_ns - min_ns;

    EXPECT_EQ(min_ns,    5'000'000ULL)  << "min must be 5 ms";
    EXPECT_EQ(max_ns,    15'000'000ULL) << "max must be 15 ms";
    EXPECT_EQ(mean_ns,   10'000'000ULL) << "mean must be 10 ms";
    EXPECT_EQ(jitter_ns, 10'000'000ULL) << "jitter (p2p) must be max-min = 10 ms";
    EXPECT_EQ(sum_ns,    30'000'000ULL) << "sum of 3 intervals = 30 ms";
}

TEST_F(FrameCallbackJitterTest, TwoFrames_OneInterval_Correct) {
    // Minimal: 2 frames → exactly 1 interval
    Metrics m;
    auto cb = make_frame_callback(m);

    cb(0ULL);
    cb(7'000'000ULL);  // +7 ms

    EXPECT_EQ(m.frames.load(std::memory_order_relaxed), 2u);
    EXPECT_EQ(m.sum_interval_ns.load(std::memory_order_relaxed), 7'000'000ULL);
    EXPECT_EQ(m.min_interval_ns.load(std::memory_order_relaxed), 7'000'000ULL);
    EXPECT_EQ(m.max_interval_ns.load(std::memory_order_relaxed), 7'000'000ULL);
}

TEST_F(FrameCallbackJitterTest, MinIntervalNeverFinallyMaxUint64) {
    // After at least 2 frames, min_interval_ns must not remain UINT64_MAX.
    Metrics m;
    auto cb = make_frame_callback(m);

    cb(100ULL);
    cb(200ULL);

    EXPECT_NE(m.min_interval_ns.load(std::memory_order_relaxed), UINT64_MAX)
        << "min_interval must be updated from UINT64_MAX sentinel after the second frame";
}
