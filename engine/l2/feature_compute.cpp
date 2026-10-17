// engine/l2/feature_compute.cpp
// EGBOS — L2 FeatureEngine production computation.
//
// This translation unit contains the SOLE production implementation of
// compute_production_features().  It MUST NOT include or call any module
// from engine/l2/candidates/ or any research-only file.
//
// RATIFIED FEATURE CONTRACTS
//   FEAT-0001: Hawkes Intensity (Feature ID: 0x0001)
//     Governance Claim: governance/claims/FEAT-0001-Hawkes-Intensity.md
//     Parameter Registry: governance/claims/PARAMETER_REGISTRY
//     Payload         : bytes 0–3, float (32-bit single-precision)
//     Validity Mask   : bit 0 (0x0001)
//     Formula         : λ(t) = μ + Σ α * exp(-β * (t - t_i))
//     Parameters      : μ = 0.1, α = 0.05, β = 0.01
//       All three parameters are sourced exclusively from
//       governance/claims/FEAT-0001-Hawkes-Intensity.md §2.
//
//   FEAT-0002: Level-1 Order Flow Imbalance (OFI) (Feature ID: 0x0002)
//     Governance Claim: governance/claims/FEAT-0002-Order-Flow-Imbalance.md
//     Parameter Registry: governance/claims/PARAMETER_REGISTRY
//     Payload         : bytes 4–7, float (32-bit single-precision)
//     Validity Mask   : bit 1 (0x0002)
//     Formula         : OFI = e_n^b - e_n^a using reconstructed absolute prices
//                       Cold start / gap = 0.0f
//
// CLOCK POLICY
//   This file MUST NOT call clock_gettime() or any equivalent.
//   The FeatureEngine queries CLOCK_MONOTONIC_RAW exactly once per
//   process_instrument() call and passes the result as pub_ts_ns.
//
// RESOLVED BLOCKERS
//   §PRICE-RECON: anchor_price (int64_t) and tick_size (int64_t) are available
//                 in Top5BidsAsks ABI v2. Reconstructed via calculate_absolute_price().
//   §FEAT-CONTRACT: Ratified via FEAT-0001-Hawkes-Intensity.md & FEAT-0002-Order-Flow-Imbalance.md.

#include "l2/feature_compute.h"
#include <cmath>
#include <cstring>

namespace egbos::l2 {

namespace {

// Per-instrument state for univariate Hawkes process (O(1) memory, zero heap allocation).
struct HawkesState {
    double   intensity{0.0};
    uint64_t last_event_ts_ns{0};
    bool     initialized{false};
};

// Per-instrument state for Level-1 OFI (O(1) memory, zero heap allocation).
struct alignas(64) OfiState {
    int64_t  prev_bid_price{0};
    uint64_t prev_bid_qty{0};
    int64_t  prev_ask_price{0};
    uint64_t prev_ask_qty{0};
    bool     initialized{false};
};

// Static fixed-size storage for all instruments (GC-E-010 compliance: zero malloc/new).
static HawkesState s_hawkes_states[egbos::shm::kMaxInstruments]{};
alignas(64) static OfiState s_ofi_states[egbos::shm::kMaxInstruments]{};

} // namespace

bool compute_production_features(
    uint32_t                       instrument_id,
    const egbos::l1::Top5BidsAsks& snapshot,
    uint64_t                       pub_ts_ns,
    FeatureComputeOutput&          out) noexcept
{
    if (instrument_id >= egbos::shm::kMaxInstruments) {
        return false;
    }

    // ── 1. Reconstruct Absolute Prices (§PRICE-RECON) ─────────────────────
    // Reconstruct best bid and ask levels from Top5BidsAsks v2 anchor_price and tick_size.
    const int64_t best_bid = snapshot.calculate_absolute_price(snapshot.bid_price_offsets[0]);
    const int64_t best_ask = snapshot.calculate_absolute_price(snapshot.ask_price_offsets[0]);
    const uint64_t bid_qty = snapshot.bid_qtys[0];
    const uint64_t ask_qty = snapshot.ask_qtys[0];

    // ── 2. FEAT-0001: Hawkes Intensity (Feature ID: 0x0001) ───────────────
    // Parameters ratified by governance/claims/FEAT-0001-Hawkes-Intensity.md §2:
    constexpr double kMu           = 0.1;    // baseline intensity μ
    constexpr double kAlpha        = 0.05;   // jump excitation α
    constexpr double kBeta         = 0.01;   // decay parameter β
    constexpr double kMaxIntensity = 1e6;    // numerical stability bound

    // Timestamp provided by caller (FeatureEngine::now_monotonic_ns()).
    // This function MUST NOT call clock_gettime() directly.
    const uint64_t now_ns = pub_ts_ns;

    HawkesState& h_state = s_hawkes_states[instrument_id];
    double lambda = 0.0;

    if (!h_state.initialized) {
        // Cold start: λ(t_0) = μ + α = 0.1 + 0.05 = 0.15
        lambda = kMu + kAlpha;
        h_state.intensity = lambda;
        h_state.last_event_ts_ns = now_ns;
        h_state.initialized = true;
    } else {
        // Exact closed-form recursive update:
        // λ(t_n^-) = μ + (λ(t_{n-1}) - μ) * exp(-β * dt)
        // λ(t_n)   = λ(t_n^-) + α
        double dt = 0.0;
        if (now_ns >= h_state.last_event_ts_ns) {
            // Convert nanoseconds → seconds for the decay exponent.
            dt = static_cast<double>(now_ns - h_state.last_event_ts_ns) * 1e-9;
        }
        const double decayed = kMu + (h_state.intensity - kMu) * std::exp(-kBeta * dt);
        lambda = decayed + kAlpha;

        if (lambda > kMaxIntensity || std::isnan(lambda)) {
            lambda = kMaxIntensity;
        }

        h_state.intensity = lambda;
        h_state.last_event_ts_ns = now_ns;
    }

    // ── 3. FEAT-0002: Level-1 Order Flow Imbalance (Feature ID: 0x0002) ────
    // Defined by governance/claims/FEAT-0002-Order-Flow-Imbalance.md:
    //   e_n^b (bid side):
    //     if curr_bid > prev_bid:   +curr_qty
    //     if curr_bid == prev_bid:  curr_qty - prev_qty
    //     if curr_bid < prev_bid:   -prev_qty
    //   e_n^a (ask side):
    //     if curr_ask < prev_ask:   -curr_qty
    //     if curr_ask == prev_ask:  prev_qty - curr_qty
    //     if curr_ask > prev_ask:   +prev_qty
    //   OFI = e_n^b - e_n^a
    // On cold start / after gap: record state and return OFI = 0.0f.
    OfiState& o_state = s_ofi_states[instrument_id];
    float ofi_val = 0.0f;

    if (!o_state.initialized) {
        // Cold start: record top of book, output 0.0f
        o_state.prev_bid_price = best_bid;
        o_state.prev_bid_qty   = bid_qty;
        o_state.prev_ask_price = best_ask;
        o_state.prev_ask_qty   = ask_qty;
        o_state.initialized    = true;
        ofi_val = 0.0f;
    } else {
        int64_t e_b = 0;
        if (best_bid > o_state.prev_bid_price) {
            e_b = static_cast<int64_t>(bid_qty);
        } else if (best_bid == o_state.prev_bid_price) {
            e_b = static_cast<int64_t>(bid_qty) - static_cast<int64_t>(o_state.prev_bid_qty);
        } else {
            e_b = -static_cast<int64_t>(o_state.prev_bid_qty);
        }

        // Ask side event contribution e_a:
        // When ask price drops (ask improves) or ask size increases at touch,
        // it represents selling pressure, which decreases OFI (e_b - e_a).
        //   best_ask < prev_ask:  e_a = +ask_qty
        //   best_ask == prev_ask: e_a = ask_qty - prev_ask_qty
        //   best_ask > prev_ask:  e_a = -prev_ask_qty
        int64_t e_a = 0;
        if (best_ask < o_state.prev_ask_price) {
            e_a = static_cast<int64_t>(ask_qty);
        } else if (best_ask == o_state.prev_ask_price) {
            e_a = static_cast<int64_t>(ask_qty) - static_cast<int64_t>(o_state.prev_ask_qty);
        } else {
            e_a = -static_cast<int64_t>(o_state.prev_ask_qty);
        }

        const int64_t ofi_int = e_b - e_a;
        ofi_val = static_cast<float>(ofi_int);

        // Update state to current top of book
        o_state.prev_bid_price = best_bid;
        o_state.prev_bid_qty   = bid_qty;
        o_state.prev_ask_price = best_ask;
        o_state.prev_ask_qty   = ask_qty;
    }

    // ── 4. Write to Payload Buffer ─────────────────────────────────────────
    // Bytes 0–3: Hawkes intensity (float)
    const float intensity_f32 = static_cast<float>(lambda);
    std::memcpy(&out.payload[0], &intensity_f32, sizeof(float));

    // Bytes 4–7: Level-1 OFI (float)
    std::memcpy(&out.payload[4], &ofi_val, sizeof(float));

    // ── 5. Update Metadata ────────────────────────────────────────────────
    out.validity_mask = (1u << 0) | (1u << 1); // Bits 0 and 1 set (mask = 3)
    out.feature_count = 2u;                   // Exactly 2 ratified features
    out.payload_bytes = 8u;                   // 8 bytes written (2 x float32)

    return true;
}

void reset_production_features(uint32_t instrument_id) noexcept {
    if (instrument_id < egbos::shm::kMaxInstruments) {
        s_hawkes_states[instrument_id] = HawkesState{};
        s_ofi_states[instrument_id]    = OfiState{};
    }
}

void reset_all_production_features() noexcept {
    for (uint32_t i = 0; i < egbos::shm::kMaxInstruments; ++i) {
        s_hawkes_states[i] = HawkesState{};
        s_ofi_states[i]    = OfiState{};
    }
}

} // namespace egbos::l2
