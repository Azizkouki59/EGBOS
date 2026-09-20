#include <memory>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../l0/ingestor.h"
#include "../l1/depth_parser.h"
#include "../l1/l1_consumer.h"
#include "../l1/lob_engine.h"
#include "../core/shm_layout.h"

using namespace egbos;
using namespace egbos::l0;
using namespace egbos::l1;
using namespace egbos::shm;
using namespace egbos::events;

namespace {

constexpr int64_t kL0Scale = 100'000LL;
constexpr int64_t kL1Scale = 10'000'000LL;

struct SemanticLevel {
    bool bid;
    int64_t price_1e7;
    uint64_t qty_1e7;
};

static SemanticLevel normalize_l0(const LobEngine::Level& x, bool bid) {
    // L0 RawMarketEvent uses 1e5 fixed-point.
    return {bid, x.price * 100LL, static_cast<uint64_t>(x.qty) * 100ULL};
}

static SemanticLevel normalize_l1(const LobEngine::Level& x, bool bid) {
    // L1 depth_parser / CLI uses 1e7 fixed-point.
    return {bid, x.price, static_cast<uint64_t>(x.qty)};
}

TEST(CliProductionParityTest, SameRawBinanceFrameProducesSameBook) {
    const char* json =
        "{\"e\":\"depthUpdate\",\"U\":1000001,\"u\":1000001,"
        "\"b\":["
        "[\"81825.96\",\"2.54763\"],"
        "[\"81825.95\",\"0.00076\"],"
        "[\"81825.94\",\"0.00013\"],"
        "[\"81824.65\",\"0.01364\"],"
        "[\"81823.34\",\"0.01637\"],"
        "[\"81823.02\",\"0.00196\"],"
        "[\"81822.31\",\"0.00007\"],"
        "[\"81822.04\",\"0.05458\"],"
        "[\"81822.03\",\"0.01637\"],"
        "[\"81821.79\",\"0.00014\"],"
        "[\"81821.78\",\"0.16595\"],"
        "[\"81821.52\",\"0.00025\"],"
        "[\"81820.85\",\"0.00013\"],"
        "[\"81820.03\",\"0.00184\"],"
        "[\"81820.02\",\"0.06163\"],"
        "[\"81820.00\",\"0.01709\"],"
        "[\"81819.04\",\"0.00611\"],"
        "[\"81819.03\",\"0.00007\"]"
        "],"
        "\"a\":["
        "[\"81828.45\",\"0.00625\"],"
        "[\"81828.40\",\"0.03666\"],"
        "[\"81828.06\",\"0.00654\"],"
        "[\"81828.02\",\"0.57433\"],"
        "[\"81828.01\",\"0.34692\"],"
        "[\"81828.00\",\"0.27703\"],"
        "[\"81827.99\",\"0.35725\"],"
        "[\"81827.98\",\"0.00014\"],"
        "[\"81827.91\",\"0.00400\"],"
        "[\"81827.82\",\"0.00200\"],"
        "[\"81827.81\",\"0.00237\"],"
        "[\"81826.49\",\"0.03096\"],"
        "[\"81826.48\",\"0.00014\"],"
        "[\"81826.10\",\"0.00014\"],"
        "[\"81826.01\",\"0.04031\"],"
        "[\"81826.00\",\"0.00014\"],"
        "[\"81825.98\",\"0.00063\"],"
        "[\"81825.97\",\"6.41168\"]"
        "]}";

    const auto* bytes = reinterpret_cast<const uint8_t*>(json);
    const std::size_t len = std::strlen(json);

    // ============================================================
    // PATH A: REAL PRODUCTION L0 -> INGESTOR -> L1 CONSUMER -> LOB
    // ============================================================
    auto ring_heap = std::make_unique<MarketEventsRegion>();
    MarketEventsRegion& ring = *ring_heap;
    GovernanceFlags gov{};

    FeedIngestor ingestor(&ring, &gov, -1);
    ingestor.process_payload(bytes, len, 123456789ULL);

    MarketEventSlot first{};
    ASSERT_TRUE(ring.pop(first));

    const auto* first_ev =
        reinterpret_cast<const RawMarketEvent*>(first.data);

    auto prod_lob =
        std::make_unique<LobEngine>(first_ev->price(), 1'000LL);

    L1Consumer consumer(ring, gov, prod_lob.get());

    prod_lob->upsert_synthetic_level(
        first_ev->price(),
        first_ev->quantity(),
        first_ev->side() == Side_BUY
    );

    uint64_t remaining = 0;
    while (ring.pop(first)) {
        ++remaining;
        consumer.process_slot(first);
    }

    EXPECT_EQ(remaining, 35u);
    EXPECT_FALSE(consumer.is_degraded());
    EXPECT_EQ(consumer.expected_next(), 1'000'002u);

    // ============================================================
    // PATH B: REAL CLI/L1 PARSER -> LobEngine
    // ============================================================
    ParsedDepthUpdate parsed{};
    ASSERT_EQ(
        parse_depth_update(bytes, len, &parsed),
        ParseResult::OK
    );

    ASSERT_EQ(parsed.first_update_id, 1'000'001u);
    ASSERT_EQ(parsed.final_update_id, 1'000'001u);
    ASSERT_EQ(parsed.bid_count, 18u);
    ASSERT_EQ(parsed.ask_count, 18u);

    auto cli_lob =
        std::make_unique<LobEngine>(
            parsed.bids[0].price,
            100'000LL
        );

    cli_lob->apply_depth_update(parsed);

    // ============================================================
    // COMPARE COMPLETE BOOK SEMANTICALLY
    // ============================================================
    std::vector<LobEngine::Level> prod_bids, prod_asks;
    std::vector<LobEngine::Level> cli_bids, cli_asks;

    prod_lob->get_book_levels(prod_bids, prod_asks, 64);
    cli_lob->get_book_levels(cli_bids, cli_asks, 64);

    ASSERT_EQ(prod_bids.size(), cli_bids.size());
    ASSERT_EQ(prod_asks.size(), cli_asks.size());

    for (std::size_t i = 0; i < prod_bids.size(); ++i) {
        const auto p = normalize_l0(prod_bids[i], true);
        const auto c = normalize_l1(cli_bids[i], true);

        EXPECT_EQ(p.price_1e7, c.price_1e7)
            << "BID price mismatch index=" << i;
        EXPECT_EQ(p.qty_1e7, c.qty_1e7)
            << "BID qty mismatch index=" << i;
    }

    for (std::size_t i = 0; i < prod_asks.size(); ++i) {
        const auto p = normalize_l0(prod_asks[i], false);
        const auto c = normalize_l1(cli_asks[i], false);

        EXPECT_EQ(p.price_1e7, c.price_1e7)
            << "ASK price mismatch index=" << i;
        EXPECT_EQ(p.qty_1e7, c.qty_1e7)
            << "ASK qty mismatch index=" << i;
    }

    // BBO semantic parity.
    EXPECT_EQ(
        prod_lob->best_bid_price() * 100LL,
        cli_lob->best_bid_price()
    );
    EXPECT_EQ(
        prod_lob->best_ask_price() * 100LL,
        cli_lob->best_ask_price()
    );

    EXPECT_EQ(
        static_cast<uint64_t>(prod_lob->best_bid_qty()) * 100ULL,
        static_cast<uint64_t>(cli_lob->best_bid_qty())
    );

    EXPECT_EQ(
        static_cast<uint64_t>(prod_lob->best_ask_qty()) * 100ULL,
        static_cast<uint64_t>(cli_lob->best_ask_qty())
    );
}

} // namespace
