// engine/tests/risk_enforcer_test.cpp
// S3 unit tests for L4 RiskEnforcer
// Spec: EGBOS-SPEC-S3-001 §4.3, §7.2
// Claims: CLAIM-L4-001 (kill switch <1ms)
// GC-E-043: L4 accesses gross_exposure_ only via CROSS_PLANE_READ

#include <gtest/gtest.h>
#include <chrono>
#include "core/risk_enforcer.h"
#include "core/position_engine.h"
#include "core/governance_interface.h"
#include "core/wal_writer.h"

using namespace egbos;
using namespace egbos::core;
using namespace egbos::wal;

static std::string tmp_path(const char* prefix) {
    std::string tmpl = std::string("/tmp/") + prefix + "_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    if (fd >= 0) close(fd);
    return std::string(buf.data());
}

class RiskEnforcerFixture : public ::testing::Test {
protected:
    void SetUp() override {
        primary = tmp_path("wal_risk_primary");
        shadow  = tmp_path("wal_risk_shadow");
        writer  = std::make_unique<WalWriter>(primary.c_str(), shadow.c_str());
        gov.pos_limit_latch.store(100000, std::memory_order_relaxed);
        gov.halt_required.store(0, std::memory_order_relaxed);
        pos_engine = std::make_unique<PositionEngine>(*writer, gov);
        risk       = std::make_unique<RiskEnforcer>(gov, *pos_engine);
    }

    std::string primary, shadow;
    std::unique_ptr<WalWriter>   writer;
    GovernanceFlags                   gov{};
    std::unique_ptr<PositionEngine>   pos_engine;
    std::unique_ptr<RiskEnforcer>     risk;
};

TEST_F(RiskEnforcerFixture, InitialStateIsInactive) {
    EXPECT_EQ(risk->get_state(), QuotingState::INACTIVE);
}

TEST_F(RiskEnforcerFixture, ApplyBreachIsMonotone) {
    // Latch starts at 100000. Lower it to 500.
    risk->apply_breach(500);
    int64_t latch = gov.pos_limit_latch.load(std::memory_order_acquire);
    EXPECT_EQ(latch, 500);

    // Attempt to raise it — must NOT take effect (monotone only decreases)
    risk->apply_breach(99999);
    latch = gov.pos_limit_latch.load(std::memory_order_acquire);
    EXPECT_EQ(latch, 500);
}

TEST_F(RiskEnforcerFixture, EvaluateRiskTriggersEmergencyOnBreach) {
    // Set latch to 0 (any exposure will breach)
    gov.pos_limit_latch.store(0, std::memory_order_release);

    // Apply a fill so exposure > 0
    FillEvent fill{1, 5, 5, 1000};
    pos_engine->apply_fill(fill);

    risk->evaluate_risk();

    EXPECT_EQ(risk->get_state(), QuotingState::EMERGENCY);
    EXPECT_EQ(gov.halt_required.load(std::memory_order_acquire), 1);
}

// ── CLAIM-L4-001: Kill switch < 1ms response (AD-010) ───────────────────────
// Spec §4.3: When exposure breaches limit, halt_required must be set within 1ms.
// This test measures the time from breach detection to halt_required=1.
TEST_F(RiskEnforcerFixture, KillSwitch_ResponseTime_Under_1ms) {
    // Set latch to 0 — any fill will exceed it
    gov.pos_limit_latch.store(0, std::memory_order_release);

    // Apply a fill to create exposure
    FillEvent fill{1, 100, 100, 1000};
    pos_engine->apply_fill(fill);

    // Measure response time of evaluate_risk() → halt_required
    auto t0 = std::chrono::steady_clock::now();
    risk->evaluate_risk();
    auto t1 = std::chrono::steady_clock::now();

    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    EXPECT_EQ(gov.halt_required.load(std::memory_order_acquire), 1)
        << "halt_required was not set after breach";
    EXPECT_LT(elapsed_us, 1000)
        << "Kill switch response " << elapsed_us << "µs exceeded 1ms budget (CLAIM-L4-001)";
}

// ── CLAIM-L4-001: Monotone latch prevents re-arming ─────────────────────────
// Once halt_required=1, apply_breach cannot raise the latch above current level.
TEST_F(RiskEnforcerFixture, HaltRequired_CannotBeCleared) {
    gov.halt_required.store(1, std::memory_order_release);
    // Attempt to raise latch (simulating re-arm) — must not succeed
    int64_t before = gov.pos_limit_latch.load(std::memory_order_acquire);
    risk->apply_breach(before + 1000); // attempt raise
    int64_t after = gov.pos_limit_latch.load(std::memory_order_acquire);
    EXPECT_LE(after, before) << "Monotone latch was raised — AD-010 violated";
}
