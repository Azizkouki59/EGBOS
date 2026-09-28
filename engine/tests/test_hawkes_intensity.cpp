// engine/tests/test_hawkes_intensity.cpp
// EGBOS — Unit tests for engine/l2/hawkes_intensity.h
//
// Governance: governance/rfcs/drafts/CLAIM-L2-HAWKES-001.yaml (status: pending)
//
// These tests use independently-computed expected values (worked by hand
// from the exponential-kernel recursive-update formula documented in
// hawkes_intensity.h), not a re-derivation of the implementation's own
// arithmetic, per the project's testing discipline.

#include <gtest/gtest.h>
#include "engine/l2/hawkes_intensity.h"
#include <cmath>

using egbos::l2::HawkesParams;
using egbos::l2::HawkesState;
using egbos::l2::HawkesStatus;
using egbos::l2::hawkes_on_event;
using egbos::l2::hawkes_reset;

namespace {

constexpr double kEps = 1e-9;

HawkesParams default_params() {
    // Illustrative only — NOT calibrated production values (RFC-003).
    return HawkesParams{
        /*mu=*/            0.001,
        /*alpha=*/         0.5,
        /*beta=*/          0.01,
        /*max_intensity=*/ 1e6,
    };
}

} // namespace

// ── Empty / cold state ──────────────────────────────────────────────────
TEST(HawkesIntensity, ColdState_IsUninitializedAndZero) {
    HawkesState state{};
    EXPECT_FALSE(state.initialized);
    EXPECT_DOUBLE_EQ(state.intensity, 0.0);
    EXPECT_EQ(state.last_event_tsc, 0u);
}

// ── Single event (warmup / cold start) ──────────────────────────────────
TEST(HawkesIntensity, FirstEvent_EqualsMuPlusAlpha) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;

    HawkesStatus status = hawkes_on_event(state, params, /*event_tsc=*/1000, out);

    ASSERT_EQ(status, HawkesStatus::OK);
    // Independently computed: cold start treats lambda_before as mu, so
    // post-jump intensity = mu + alpha = 0.001 + 0.5 = 0.501.
    EXPECT_NEAR(out, 0.501, kEps);
    EXPECT_TRUE(state.initialized);
    EXPECT_EQ(state.last_event_tsc, 1000u);
    EXPECT_NEAR(state.intensity, 0.501, kEps);
}

// ── Multiple events ──────────────────────────────────────────────────────
TEST(HawkesIntensity, SecondEvent_DecaysThenJumps) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;

    ASSERT_EQ(hawkes_on_event(state, params, 1000, out), HawkesStatus::OK);
    // lambda after event 1 = 0.501 at t=1000.

    ASSERT_EQ(hawkes_on_event(state, params, 1100, out), HawkesStatus::OK);
    // Independently computed:
    //   dt = 100
    //   decayed = mu + (0.501 - mu) * exp(-beta*dt)
    //           = 0.001 + 0.5 * exp(-0.01 * 100)
    //           = 0.001 + 0.5 * exp(-1)
    //           = 0.001 + 0.5 * 0.36787944117144233
    //           = 0.001 + 0.18393972058572117
    //           = 0.18493972058572117
    //   lambda_after = decayed + alpha = 0.68493972058572117
    const double expected = 0.001 + 0.5 * std::exp(-1.0) + 0.5;
    EXPECT_NEAR(out, expected, 1e-12);
}

// ── Equal timestamps (dt = 0) ────────────────────────────────────────────
TEST(HawkesIntensity, EqualTimestamps_NoDecayJumpOnly) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;

    ASSERT_EQ(hawkes_on_event(state, params, 5000, out), HawkesStatus::OK);
    const double after_first = out; // 0.501

    ASSERT_EQ(hawkes_on_event(state, params, 5000, out), HawkesStatus::OK);
    // dt=0 => exp(0)=1 => decayed = mu + (lambda-mu)*1 = lambda unchanged,
    // then + alpha again.
    EXPECT_NEAR(out, after_first + params.alpha, kEps);
}

// ── Tiny delta-t ─────────────────────────────────────────────────────────
TEST(HawkesIntensity, TinyDeltaT_DecayNearlyOne) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;

    ASSERT_EQ(hawkes_on_event(state, params, 1'000'000, out), HawkesStatus::OK);
    const double after_first = out;

    ASSERT_EQ(hawkes_on_event(state, params, 1'000'001, out), HawkesStatus::OK); // dt=1ns
    const double expected = params.mu + (after_first - params.mu) * std::exp(-params.beta * 1.0) + params.alpha;
    EXPECT_NEAR(out, expected, 1e-12);
    // Sanity: with beta=0.01 and dt=1, decay factor is ~0.99, so intensity
    // should be very close to (after_first - mu) + alpha, not fully decayed.
    EXPECT_GT(out, after_first); // still growing due to alpha jump
}

// ── Large delta-t ────────────────────────────────────────────────────────
TEST(HawkesIntensity, LargeDeltaT_DecaysToBaselinePlusAlpha) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;

    ASSERT_EQ(hawkes_on_event(state, params, 0, out), HawkesStatus::OK);
    ASSERT_EQ(hawkes_on_event(state, params, 1'000'000'000ull /*1s in ns, huge given beta=0.01/ns*/, out), HawkesStatus::OK);
    // With beta*dt this large, exp(-beta*dt) underflows to 0.0 in double
    // precision, so decayed collapses to mu exactly; lambda_after = mu + alpha.
    EXPECT_NEAR(out, params.mu + params.alpha, 1e-12);
}

// ── Non-monotonic timestamp ──────────────────────────────────────────────
TEST(HawkesIntensity, NonMonotonicTime_ReturnsInvalidAndDoesNotMutateOutput) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;

    ASSERT_EQ(hawkes_on_event(state, params, 2000, out), HawkesStatus::OK);
    const double before = out;
    const HawkesState state_before = state;

    out = -999.0; // sentinel
    HawkesStatus status = hawkes_on_event(state, params, 1999 /* went backward */, out);

    EXPECT_EQ(status, HawkesStatus::INVALID_NONMONOTONIC_TIME);
    EXPECT_DOUBLE_EQ(out, -999.0); // output untouched on invalid status
    EXPECT_DOUBLE_EQ(state.intensity, state_before.intensity); // state untouched
    EXPECT_EQ(state.last_event_tsc, state_before.last_event_tsc);
    (void)before;
}

// ── Boundary: dt exactly at uint64 wrap-adjacent large value ────────────
TEST(HawkesIntensity, VeryLargeButValidDeltaT_DoesNotOverflowComputation) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;

    ASSERT_EQ(hawkes_on_event(state, params, 0, out), HawkesStatus::OK);
    // A very large but monotonic dt should decay cleanly to baseline, not
    // produce NaN/inf from the exp() argument.
    HawkesStatus status = hawkes_on_event(state, params, 1'000'000'000'000ull, out);
    ASSERT_EQ(status, HawkesStatus::OK);
    EXPECT_TRUE(std::isfinite(out));
    EXPECT_NEAR(out, params.mu + params.alpha, 1e-12);
}

// ── Reset ─────────────────────────────────────────────────────────────────
TEST(HawkesIntensity, Reset_ReturnsToColdState) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;

    ASSERT_EQ(hawkes_on_event(state, params, 100, out), HawkesStatus::OK);
    ASSERT_TRUE(state.initialized);

    hawkes_reset(state);

    EXPECT_FALSE(state.initialized);
    EXPECT_DOUBLE_EQ(state.intensity, 0.0);
    EXPECT_EQ(state.last_event_tsc, 0u);

    // Next event after reset behaves exactly like a true first event.
    ASSERT_EQ(hawkes_on_event(state, params, 5, out), HawkesStatus::OK);
    EXPECT_NEAR(out, params.mu + params.alpha, kEps);
}

// ── Deterministic replay ──────────────────────────────────────────────────
TEST(HawkesIntensity, DeterministicReplay_SameInputsSameOutputs) {
    HawkesParams params = default_params();
    const uint64_t timestamps[] = {0, 50, 50, 200, 1000, 1000000, 1000050};

    auto run = [&]() {
        HawkesState state{};
        double out = 0.0;
        double last = -1.0;
        for (uint64_t ts : timestamps) {
            auto status = hawkes_on_event(state, params, ts, out);
            EXPECT_EQ(status, HawkesStatus::OK);
            last = out;
        }
        return last;
    };

    const double run1 = run();
    const double run2 = run();
    EXPECT_DOUBLE_EQ(run1, run2); // bit-identical, not just "close"
}

// ── Numerical stability: overflow ────────────────────────────────────────
TEST(HawkesIntensity, Overflow_ReturnsInvalidWithoutMutatingState) {
    HawkesState state{};
    HawkesParams params = default_params();
    params.max_intensity = 0.4; // deliberately below mu+alpha (0.501)
    double out = -999.0;

    HawkesStatus status = hawkes_on_event(state, params, 10, out);

    EXPECT_EQ(status, HawkesStatus::INVALID_OVERFLOW);
    EXPECT_DOUBLE_EQ(out, -999.0);       // output untouched
    EXPECT_FALSE(state.initialized);      // state untouched (still cold)
}

TEST(HawkesIntensity, Overflow_OnSubsequentEvent_DoesNotMutateState) {
    HawkesState state{};
    HawkesParams params = default_params();
    double out = 0.0;
    ASSERT_EQ(hawkes_on_event(state, params, 10, out), HawkesStatus::OK); // 0.501

    HawkesState state_before = state;
    params.max_intensity = 0.5; // now below current+alpha for the next jump
    out = -999.0;

    HawkesStatus status = hawkes_on_event(state, params, 20, out);

    EXPECT_EQ(status, HawkesStatus::INVALID_OVERFLOW);
    EXPECT_DOUBLE_EQ(out, -999.0);
    EXPECT_DOUBLE_EQ(state.intensity, state_before.intensity);
    EXPECT_EQ(state.last_event_tsc, state_before.last_event_tsc);
}
