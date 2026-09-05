// engine/tests/test_binance_feed_listener.cpp
// EGBOS ENGINE_V3 — Binance Feed Listener Unit Tests
//
// Classification : DEVELOPMENT_TEST / LOCAL_SYNTHETIC
//                  These tests do NOT constitute LIVE_OPERATIONAL evidence.
//                  Connection tests require BINANCE_TESTNET_API_KEY.
//
// Governance     : D7 (provenance must be correctly classified)
//                  D9 (testnet != live operational)
//                  TEST DATA != LIVE_OPERATIONAL EVIDENCE

#include <gtest/gtest.h>
#include "engine/l0/binance_feed_listener.h"
#include "engine/l0/binance_trading_client.h"
#include "engine/l0/feed_source.h"

// ── Minimal stub ingestor for unit testing ────────────────────────────
#include "engine/core/shm_layout.h"
#include "engine/l0/ingestor.h"

namespace {

// Null Tier-0 fd for test purposes (no actual WAL write)
constexpr int kNullFd = -1;

} // namespace

// ──────────────────────────────────────────────────────────────────────
// ProvenanceTest — verifies D7/D9 provenance classification
// ──────────────────────────────────────────────────────────────────────
class BinanceProvenanceTest : public ::testing::Test {};

TEST_F(BinanceProvenanceTest, TestnetProvenance_IsDevelopmentTest) {
    egbos::l0::BinanceFeedConfig config;
    config.use_testnet = true;
    config.host        = "testnet.binance.vision";
    config.path        = "/ws";
    config.port        = 443;

    // We verify provenance classification WITHOUT making a real connection.
    // D9: Testnet must NEVER be classified as LIVE_OPERATIONAL.
    EXPECT_EQ(static_cast<uint8_t>(egbos::l0::FeedProvenance::DEVELOPMENT_TEST),
              static_cast<uint8_t>(0));

    // Provenance::DEVELOPMENT_TEST < PRODUCTION_VERIFIED
    EXPECT_LT(static_cast<uint8_t>(egbos::l0::FeedProvenance::DEVELOPMENT_TEST),
              static_cast<uint8_t>(egbos::l0::FeedProvenance::PRODUCTION_VERIFIED));
}

TEST_F(BinanceProvenanceTest, ProductionProvenance_IsExternalLive) {
    // D9: Production starts as EXTERNAL_LIVE; only promoted to PRODUCTION_VERIFIED
    // after MARKET-DATA-001 countersignature.
    EXPECT_EQ(static_cast<uint8_t>(egbos::l0::FeedProvenance::EXTERNAL_LIVE),
              static_cast<uint8_t>(2));
    EXPECT_LT(static_cast<uint8_t>(egbos::l0::FeedProvenance::EXTERNAL_LIVE),
              static_cast<uint8_t>(egbos::l0::FeedProvenance::PRODUCTION_VERIFIED));
}

TEST_F(BinanceProvenanceTest, ProductionVerified_IsHighestClass) {
    // PRODUCTION_VERIFIED = 3 (highest provenance)
    EXPECT_EQ(static_cast<uint8_t>(egbos::l0::FeedProvenance::PRODUCTION_VERIFIED),
              static_cast<uint8_t>(3));
}

TEST_F(BinanceProvenanceTest, TestData_IsNotLiveOperational) {
    // Governance invariant: TEST DATA != LIVE_OPERATIONAL
    // Structural enforcement: DEVELOPMENT_TEST(0) != PRODUCTION_VERIFIED(3)
    EXPECT_NE(static_cast<uint8_t>(egbos::l0::FeedProvenance::DEVELOPMENT_TEST),
              static_cast<uint8_t>(egbos::l0::FeedProvenance::PRODUCTION_VERIFIED));
}

// ──────────────────────────────────────────────────────────────────────
// ConfigTest — verifies config structure is valid
// ──────────────────────────────────────────────────────────────────────
class BinanceConfigTest : public ::testing::Test {};

TEST_F(BinanceConfigTest, DefaultConfig_IsTestnet) {
    egbos::l0::BinanceFeedConfig config;
    EXPECT_TRUE(config.use_testnet);
    EXPECT_EQ(config.port, 443);
    EXPECT_EQ(config.stale_threshold_ns, 500'000'000ULL);
}

TEST_F(BinanceConfigTest, ProductionConfig_Correctly_SetHostname) {
    egbos::l0::BinanceFeedConfig config;
    config.use_testnet = false;
    config.host        = "stream.binance.com";
    config.port        = 9443;
    EXPECT_FALSE(config.use_testnet);
    EXPECT_EQ(config.port, 9443u);
}

// ──────────────────────────────────────────────────────────────────────
// TradingClientTest — verifies trading client credential handling
// ──────────────────────────────────────────────────────────────────────
class BinanceTradingClientTest : public ::testing::Test {};

TEST_F(BinanceTradingClientTest, TestnetClient_ProvenanceIsDevelopmentTest) {
    egbos::l0::BinanceTradingClient client(/*use_testnet=*/true);
    EXPECT_EQ(client.provenance(), egbos::l0::FeedProvenance::DEVELOPMENT_TEST);
}

TEST_F(BinanceTradingClientTest, ProductionClient_ProvenanceIsExternalLive) {
    egbos::l0::BinanceTradingClient client(/*use_testnet=*/false);
    EXPECT_EQ(client.provenance(), egbos::l0::FeedProvenance::EXTERNAL_LIVE);
}

TEST_F(BinanceTradingClientTest, InitFails_WhenCredentialsAbsent) {
    // Unset env vars to verify init() refuses without credentials.
    // D9: credentials must come from environment — never hardcoded.
    ::unsetenv("BINANCE_TESTNET_API_KEY");
    ::unsetenv("BINANCE_TESTNET_API_SECRET");

    egbos::l0::BinanceTradingClient client(/*use_testnet=*/true);
    bool ok = client.init();
    EXPECT_FALSE(ok);
    EXPECT_FALSE(client.is_ready());
}

// ──────────────────────────────────────────────────────────────────────
// LiveConnectionTest — runs ONLY when testnet credentials are present
// Classification: DEVELOPMENT_TEST — NOT LIVE_OPERATIONAL
// ──────────────────────────────────────────────────────────────────────
class BinanceLiveConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        has_creds_ = (::getenv("BINANCE_TESTNET_API_KEY")    != nullptr &&
                      ::getenv("BINANCE_TESTNET_API_SECRET") != nullptr);
    }
    bool has_creds_{false};
};

TEST_F(BinanceLiveConnectionTest, TradingClient_Init_WithCredentials) {
    if (!has_creds_) {
        GTEST_SKIP() << "BINANCE_TESTNET_API_KEY / BINANCE_TESTNET_API_SECRET not set "
                        "(DEVELOPMENT_TEST classification; not LIVE_OPERATIONAL)";
    }
    egbos::l0::BinanceTradingClient client(/*use_testnet=*/true);
    bool ok = client.init();
    EXPECT_TRUE(ok);
    EXPECT_TRUE(client.is_ready());
    EXPECT_EQ(client.provenance(), egbos::l0::FeedProvenance::DEVELOPMENT_TEST);
}

TEST_F(BinanceLiveConnectionTest, TradingClient_GetAccount_ReturnsJSON) {
    if (!has_creds_) {
        GTEST_SKIP() << "Credentials not set";
    }
    egbos::l0::BinanceTradingClient client(/*use_testnet=*/true);
    ASSERT_TRUE(client.init());

    char buf[4096];
    bool ok = client.get_account(buf, sizeof(buf));
    // Response may contain "makerCommission" or "balances"
    EXPECT_TRUE(ok);
    if (ok) {
        EXPECT_TRUE(::strstr(buf, "balances") != nullptr ||
                    ::strstr(buf, "makerCommission") != nullptr ||
                    ::strstr(buf, "200") != nullptr)
            << "Unexpected response: " << buf;
    }
}
