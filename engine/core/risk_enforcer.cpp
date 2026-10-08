// engine/core/risk_enforcer.cpp
// EGBOS ENGINE_V3 — L4 Risk Enforcer

#include "risk_enforcer.h"

namespace egbos::core {

RiskEnforcer::RiskEnforcer(shm::GovernanceFlags& gov_flags, shm::RiskLimitsRegion& risk_limits, PositionEngine& pos_engine)
    : governance_flags_(gov_flags), risk_limits_(risk_limits), position_engine_(pos_engine) {}

void RiskEnforcer::evaluate_risk() {
    // Spec §4.3 and §7.1: read gross_exposure_ via CROSS_PLANE_READ
    int64_t exposure = CROSS_PLANE_READ(
        position_engine_.gross_exposure_,
        egbos::plane::LAYER_L4,
        egbos::plane::LAYER_L3
    );

    // If exposure breaches latch, transition to HEDGING/EMERGENCY
    int64_t limit = risk_limits_.pos_limit.load(std::memory_order_acquire);
    if (exposure > limit) {
        apply_breach(0); // Immediately reduce latch
        transition_state(QuotingState::EMERGENCY);
    }
}

void RiskEnforcer::apply_breach(int64_t new_ceiling) {
    int64_t current = risk_limits_.pos_limit.load(std::memory_order_acquire);
    // Monotone: only write if new_ceiling is strictly lower
    while (new_ceiling < current) {
        if (risk_limits_.pos_limit.compare_exchange_weak(
                current, new_ceiling,
                std::memory_order_release,
                std::memory_order_acquire)) {
            break; // Latch lowered
        }
    }
}

void RiskEnforcer::transition_state(QuotingState new_state) {
    // Valid transitions logic goes here.
    // L4B state machine rules:
    // INACTIVE -> QUOTING
    // QUOTING -> HEDGING -> TAKER_REDUCE
    // QUOTING/HEDGING -> EMERGENCY
    
    // Simplistic implementation for the test harness:
    if (new_state == QuotingState::EMERGENCY) {
        governance_flags_.halt_required.store(true, std::memory_order_release);
    }
    current_state_ = new_state;
}

void RiskEnforcer::watchdog_tick() {
    // Spec: If the heartbeat misses for > 50µs, or a manual kill signal is received,
    // the FPGA's logic immediately shuts off power to the SFP+ transceiver.
    // Software path: kill switch < 1ms response.
}

void RiskEnforcer::snapshot_into(RiskSnapshot& out) const noexcept {
    out.current_state = static_cast<uint32_t>(current_state_);
}

void RiskEnforcer::hydrate_from(const RiskSnapshot& in) noexcept {
    current_state_ = static_cast<QuotingState>(in.current_state);
    last_heartbeat_tsc_ = 0;
}

} // namespace egbos::core
