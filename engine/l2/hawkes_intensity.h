// engine/l2/hawkes_intensity.h
// EGBOS — L2 Hawkes self-exciting intensity (exponential kernel)
//
// Governance   : governance/rfcs/drafts/CLAIM-L2-HAWKES-001.yaml (status: pending)
// Scoping RFC  : governance/rfcs/RFC-003-L2-HAWKES-VPIN-FEATURE-SCOPING.yaml
//
// STATUS: NOT EGBOS-APPROVED. This computes a value; it is not wired into
// FeatureEngine::process_instrument() and does not populate any
// FeatureVector payload byte until CLAIM-L2-HAWKES-001 moves to "active"
// with review_authority sign-off. See that claim for enforcement_gaps
// (no offline reference implementation yet; parameters uncalibrated).
//
// ── Definition ───────────────────────────────────────────────────────────
// Univariate Hawkes process, exponential excitation kernel, exact
// (non-discretized) recursive update. Event source: L0 depth-event stream
// (RawMarketEvent, EventType in {NEW, CANCEL}), pooled across bid/ask sides,
// per instrument. Timestamp source: ingestion_tsc (CLOCK_MONOTONIC_RAW at
// WebSocket frame receipt). exchange_tsc is NOT used (always 0 on the
// @depth feed; see RFC-003 evidence).
//
//   lambda(t) = mu + sum_{t_i < t} alpha * exp(-beta * (t - t_i))
//
// Exact recursive update at each new event arriving at time t_n (standard
// closed-form update for an exponential-kernel Hawkes process — this
// avoids storing event history, satisfying GC-E-010 zero-allocation):
//
//   lambda(t_n^-) = mu + (lambda(t_{n-1}) - mu) * exp(-beta * (t_n - t_{n-1}))
//   lambda(t_n)   = lambda(t_n^-) + alpha        [post-jump, after event n]
//
// State stored between events is lambda(t_{n-1}) (post-jump intensity after
// the previous event) and t_{n-1} itself. No event history is retained.
//
// ── Open parameters (RFC-003; NOT calibrated) ───────────────────────────
// mu, alpha, beta are supplied externally via HawkesParams — this file does
// not hardcode them. Values in tests are illustrative only, not calibrated
// production values.
//
// ── Explicit unresolved/decided policy points (recorded, not invented) ──
//   - Non-monotonic timestamp (dt < 0): returns INVALID_NONMONOTONIC_TIME.
//     Never silently treated as dt=0 or abs(dt), since ingestion_tsc going
//     backward indicates a caller/clock bug, not a valid input.
//   - Overflow (accumulated intensity exceeds HawkesParams.max_intensity):
//     returns INVALID_OVERFLOW. Never silently clamped, per the same
//     explicit-invalidity discipline used elsewhere in this codebase
//     (see egbos::l0::L0ParseStatus for the established convention).
//   - First event / cold state: lambda_before is treated as mu (no prior
//     excitation), so the first event's post-jump intensity is mu + alpha.
//   - Reset (hawkes_reset): sets state back to the cold/uninitialized form.
//     Callers MUST invoke this on GapEvent, mirroring the LOB engine's own
//     book-clear-on-gap behavior (EGBOS_MASTER.md "Gap Event Emission").
//     This file does not itself observe GapEvent; wiring that call is the
//     responsibility of the (not-yet-written) integration into
//     FeatureEngine::process_instrument(), tracked under
//     CLAIM-L2-HAWKES-001 enforcement_gaps.

#pragma once

#include <cstdint>
#include <cmath>

namespace egbos::l2 {

// ── HawkesStatus ────────────────────────────────────────────────────────
// Mirrors the egbos::l0::L0ParseStatus convention: status returned by
// value, output written via reference out-parameter, noexcept.
enum class HawkesStatus : uint8_t {
    OK                          = 0,
    INVALID_NONMONOTONIC_TIME   = 1,  // dt < 0: ingestion_tsc went backward
    INVALID_OVERFLOW            = 2,  // accumulated intensity exceeded max_intensity
};

// ── HawkesParams ────────────────────────────────────────────────────────
// External configuration — NOT hardcoded. No calibrated values are known
// yet (RFC-003 open_parameters). Units: mu/alpha are in "events per
// nanosecond" if ingestion_tsc is nanoseconds (matches CLOCK_MONOTONIC_RAW
// convention used elsewhere in engine/l0); beta is in inverse nanoseconds.
struct HawkesParams {
    double mu;             // baseline intensity, mu > 0
    double alpha;          // excitation jump per event, alpha >= 0
    double beta;           // decay rate, beta > 0
    double max_intensity;  // numerical safety bound; exceeding it is INVALID_OVERFLOW
};

// ── HawkesState ─────────────────────────────────────────────────────────
// Per-instrument state. All-zero / initialized=false is the cold state.
struct HawkesState {
    double   intensity{0.0};       // lambda(t_{n-1}), post-jump, after last event
    uint64_t last_event_tsc{0};
    bool     initialized{false};
};

// ── hawkes_reset ────────────────────────────────────────────────────────
// Returns state to the cold/uninitialized form. Callers MUST invoke this
// on GapEvent (see file header). Does not itself read params; the next
// call to hawkes_on_event will re-initialize using its own params.mu.
inline void hawkes_reset(HawkesState& state) noexcept {
    state.intensity = 0.0;
    state.last_event_tsc = 0;
    state.initialized = false;
}

// ── hawkes_on_event ─────────────────────────────────────────────────────
// Advances the Hawkes state by one depth event arriving at event_tsc
// (ingestion_tsc, nanoseconds). Writes the new post-jump intensity to
// *out_intensity on HawkesStatus::OK. On any non-OK status, *out_intensity
// is left unmodified — callers must check the return status before using
// the output, matching the ComputeStatus/Result discipline used elsewhere
// for denominator-zero-class conditions.
[[nodiscard]] inline HawkesStatus hawkes_on_event(
    HawkesState&        state,
    const HawkesParams& params,
    uint64_t             event_tsc,
    double&              out_intensity) noexcept
{
    if (!state.initialized) {
        // Cold start: lambda_before is treated as mu (no prior excitation).
        // First event's post-jump intensity is mu + alpha.
        double lambda_after = params.mu + params.alpha;
        if (!(lambda_after <= params.max_intensity)) {
            // NaN-safe: '!(<=)' catches both overflow and NaN.
            return HawkesStatus::INVALID_OVERFLOW;
        }
        state.intensity = lambda_after;
        state.last_event_tsc = event_tsc;
        state.initialized = true;
        out_intensity = lambda_after;
        return HawkesStatus::OK;
    }

    if (event_tsc < state.last_event_tsc) {
        return HawkesStatus::INVALID_NONMONOTONIC_TIME;
    }

    const double dt = static_cast<double>(event_tsc - state.last_event_tsc);
    const double decayed = params.mu + (state.intensity - params.mu) * std::exp(-params.beta * dt);
    const double lambda_after = decayed + params.alpha;

    if (!(lambda_after <= params.max_intensity)) {
        return HawkesStatus::INVALID_OVERFLOW;
    }

    state.intensity = lambda_after;
    state.last_event_tsc = event_tsc;
    out_intensity = lambda_after;
    return HawkesStatus::OK;
}

} // namespace egbos::l2
