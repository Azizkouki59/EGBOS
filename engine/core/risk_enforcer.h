// engine/core/risk_enforcer.h
// EGBOS ENGINE_V3 — L4 Risk Enforcer
// Specification: EGBOS-SPEC-S3-001

#pragma once

#include <atomic>
#include <cstdint>
#include "governance_interface.h"
#include "cross_plane.h"
#include "position_engine.h"

namespace egbos::core {

enum class QuotingState {
    INACTIVE,
    QUOTING,
    HEDGING,
    TAKER_REDUCE,
    EMERGENCY
};

class RiskEnforcer {
public:
    explicit RiskEnforcer(GovernanceFlags& gov_flags, PositionEngine& pos_engine);
    ~RiskEnforcer() = default;

    // Disallow copy/move
    RiskEnforcer(const RiskEnforcer&) = delete;
    RiskEnforcer& operator=(const RiskEnforcer&) = delete;

    void evaluate_risk();
    void apply_breach(int64_t new_ceiling);
    void transition_state(QuotingState new_state);
    void watchdog_tick();

    QuotingState get_state() const { return current_state_; }

private:
    GovernanceFlags& governance_flags_;
    PositionEngine& position_engine_; // L4 has reference but only uses CROSS_PLANE_READ

    QuotingState current_state_{QuotingState::INACTIVE};
    uint64_t last_heartbeat_tsc_{0};
};

} // namespace egbos::core
