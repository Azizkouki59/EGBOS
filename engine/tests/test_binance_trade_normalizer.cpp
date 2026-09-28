#include <gtest/gtest.h>

#include "engine/l0/binance_json_normalizer.h"

#include <cstdint>
#include <cstring>
#include <limits>

namespace {

struct Captured {
    bool called = false;
    egbos::events::RawMarketEvent event{};
};

bool capture(
    const egbos::events::RawMarketEvent& event,
    void* user_ctx) noexcept
{
    auto* out = static_cast<Captured*>(user_ctx);
    out->event = event;
    out->called = true;
    return true;
}

egbos::l0::L0ParseStatus parse(
    const char* json,
    Captured& captured)
{
    return egbos::l0::normalize_binance_trade(
        reinterpret_cast<const uint8_t*>(json),
        std::strlen(json),
        123456789ULL,
        capture,
        &captured);
}

} // namespace

TEST(BinanceTradeNormalizerTest, ParsesProductionWireShape) {
    const char* json =
        R"({"e":"trade","E":1790509401329,"s":"BTCUSDT","t":6715989461,"p":"84894.01000000","q":"0.00028000","T":1790509401329,"m":false,"M":true})";

    Captured c{};
    ASSERT_EQ(parse(json, c), egbos::l0::L0ParseStatus::OK);
    ASSERT_TRUE(c.called);

    EXPECT_EQ(c.event.event_type(), egbos::events::EventType_TRADE);
    EXPECT_EQ(c.event.side(), egbos::events::Side_BUY);
    EXPECT_EQ(c.event.quantity(), 28U);
    EXPECT_EQ(c.event.sequence_num(), 6715989461ULL);
    EXPECT_EQ(c.event.price(), 8489401000LL);
    EXPECT_EQ(c.event.exchange_tsc(), 1790509401329000000ULL);
    EXPECT_EQ(c.event.ingestion_tsc(), 123456789ULL);
}

TEST(BinanceTradeNormalizerTest, MakerTrueMapsToSellAggressor) {
    const char* json =
        R"({"e":"trade","s":"BTCUSDT","t":1,"p":"100.00000","q":"1.00000","T":2,"m":true})";

    Captured c{};
    ASSERT_EQ(parse(json, c), egbos::l0::L0ParseStatus::OK);
    EXPECT_EQ(c.event.event_type(), egbos::events::EventType_TRADE);
    EXPECT_EQ(c.event.side(), egbos::events::Side_SELL);
}

TEST(BinanceTradeNormalizerTest, RejectsUnsupportedSymbol) {
    const char* json =
        R"({"e":"trade","s":"ETHUSDT","t":1,"p":"100.00000","q":"1.00000","T":2,"m":false})";

    Captured c{};
    EXPECT_EQ(parse(json, c), egbos::l0::L0ParseStatus::MISSING_FIELD);
    EXPECT_FALSE(c.called);
}

TEST(BinanceTradeNormalizerTest, RejectsWrongEventType) {
    const char* json =
        R"({"e":"depthUpdate","s":"BTCUSDT","t":1,"p":"100.00000","q":"1.00000","T":2,"m":false})";

    Captured c{};
    EXPECT_EQ(parse(json, c), egbos::l0::L0ParseStatus::WRONG_EVENT_TYPE);
    EXPECT_FALSE(c.called);
}

TEST(BinanceTradeNormalizerTest, RejectsMissingRequiredField) {
    const char* json =
        R"({"e":"trade","s":"BTCUSDT","t":1,"p":"100.00000","q":"1.00000","m":false})";

    Captured c{};
    EXPECT_EQ(parse(json, c), egbos::l0::L0ParseStatus::MISSING_FIELD);
    EXPECT_FALSE(c.called);
}

TEST(BinanceTradeNormalizerTest, RejectsTruncatedFrame) {
    const char* json =
        R"({"e":"trade","s":"BTCUSDT","t":1,"p":"100.00000","q":"1.00000","T":2,"m":false)";

    Captured c{};
    EXPECT_EQ(parse(json, c), egbos::l0::L0ParseStatus::TRUNCATED);
    EXPECT_FALSE(c.called);
}

TEST(BinanceTradeNormalizerTest, RejectsQuantityOverflow) {
    const char* json =
        R"({"e":"trade","s":"BTCUSDT","t":1,"p":"100.00000","q":"42949.67296","T":2,"m":false})";

    Captured c{};
    EXPECT_EQ(parse(json, c), egbos::l0::L0ParseStatus::OVERFLOW);
    EXPECT_FALSE(c.called);
}

TEST(BinanceTradeNormalizerTest, RejectsScientificNotation) {
    const char* json =
        R"({"e":"trade","s":"BTCUSDT","t":1,"p":"1e2","q":"1.00000","T":2,"m":false})";

    Captured c{};
    EXPECT_EQ(parse(json, c), egbos::l0::L0ParseStatus::SCIENTIFIC_NOTATION);
    EXPECT_FALSE(c.called);
}

static_assert(sizeof(egbos::events::RawMarketEvent) == 64,
              "Trade normalization must preserve the existing 64-byte ABI");
