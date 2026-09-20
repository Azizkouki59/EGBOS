#include <memory>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>

#include "../l0/ingestor.h"
#include "../l1/l1_consumer.h"
#include "../l1/lob_engine.h"
#include "../core/shm_layout.h"

using namespace egbos;
using namespace egbos::events;
using namespace egbos::l0;
using namespace egbos::l1;
using namespace egbos::shm;

namespace {

struct GoldenLevel {
    bool is_bid;
    int64_t price;
    uint32_t qty;
};

static constexpr int64_t kScale = 100'000LL;
static constexpr int64_t kTick  = 1'000LL; // 0.01 in L0 RawMarketEvent scale

static int64_t P(const char* s) {
    std::string x(s);
    std::size_t dot = x.find('.');
    int64_t whole = std::stoll(dot == std::string::npos ? x : x.substr(0, dot));

    int64_t frac = 0;
    if (dot != std::string::npos) {
        std::string f = x.substr(dot + 1);

        // L0 RawMarketEvent uses 10^5 fixed-point.
        if (f.size() > 5) {
            for (std::size_t i = 5; i < f.size(); ++i) {
                if (f[i] != '0') {
                    throw std::runtime_error("P(): precision exceeds L0 10^5 scale");
                }
            }
            f.resize(5);
        }

        while (f.size() < 5) f.push_back('0');
        frac = std::stoll(f);
    }

    return whole * kScale + frac;
}

static uint32_t Q(const char* s) {
    int64_t v = P(s);
    if (v < 0 || v > static_cast<int64_t>(UINT32_MAX)) {
        throw std::runtime_error("Q(): quantity outside uint32 range");
    }
    return static_cast<uint32_t>(v);
}

TEST(ExactMarketPipelineTest, ScreenshotBookPreservesExactValuesEndToEnd) {
    /*
     * Golden values from the observed Binance book.
     * One Binance depth frame => multiple L0 slots, all carrying the same U/u.
     */
    const uint64_t U = 1'000'001;
    const uint64_t u = 1'000'001;

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

    auto ring_ingest_heap = std::make_unique<MarketEventsRegion>();
    MarketEventsRegion& ring_ingest = *ring_ingest_heap;
    auto ring_consume_heap = std::make_unique<MarketEventsRegion>();
    MarketEventsRegion& ring_consume = *ring_consume_heap;
    GovernanceFlags gov{};

    FeedIngestor ingestor(&ring_ingest, &gov, -1);

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(json);
    ingestor.process_payload(bytes, std::strlen(json), 123456789ULL);

    std::vector<MarketEventSlot> slots;

    MarketEventSlot slot{};
    while (ring_ingest.pop(slot)) {
        slots.push_back(slot);
    }

    ASSERT_EQ(slots.size(), 36u);

    // Verify the exact L0 output BEFORE L1 touches it.
    const std::vector<GoldenLevel> golden = {
        // bids
        {true,  P("81825.96"), Q("2.54763")},
        {true,  P("81825.95"), Q("0.00076")},
        {true,  P("81825.94"), Q("0.00013")},
        {true,  P("81824.65"), Q("0.01364")},
        {true,  P("81823.34"), Q("0.01637")},
        {true,  P("81823.02"), Q("0.00196")},
        {true,  P("81822.31"), Q("0.00007")},
        {true,  P("81822.04"), Q("0.05458")},
        {true,  P("81822.03"), Q("0.01637")},
        {true,  P("81821.79"), Q("0.00014")},
        {true,  P("81821.78"), Q("0.16595")},
        {true,  P("81821.52"), Q("0.00025")},
        {true,  P("81820.85"), Q("0.00013")},
        {true,  P("81820.03"), Q("0.00184")},
        {true,  P("81820.02"), Q("0.06163")},
        {true,  P("81820.00"), Q("0.01709")},
        {true,  P("81819.04"), Q("0.00611")},
        {true,  P("81819.03"), Q("0.00007")},

        // asks
        {false, P("81828.45"), Q("0.00625")},
        {false, P("81828.40"), Q("0.03666")},
        {false, P("81828.06"), Q("0.00654")},
        {false, P("81828.02"), Q("0.57433")},
        {false, P("81828.01"), Q("0.34692")},
        {false, P("81828.00"), Q("0.27703")},
        {false, P("81827.99"), Q("0.35725")},
        {false, P("81827.98"), Q("0.00014")},
        {false, P("81827.91"), Q("0.00400")},
        {false, P("81827.82"), Q("0.00200")},
        {false, P("81827.81"), Q("0.00237")},
        {false, P("81826.49"), Q("0.03096")},
        {false, P("81826.48"), Q("0.00014")},
        {false, P("81826.10"), Q("0.00014")},
        {false, P("81826.01"), Q("0.04031")},
        {false, P("81826.00"), Q("0.00014")},
        {false, P("81825.98"), Q("0.00063")},
        {false, P("81825.97"), Q("6.41168")}
    };

    ASSERT_EQ(golden.size(), slots.size());

    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto* ev =
            reinterpret_cast<const RawMarketEvent*>(slots[i].data);
        const auto& meta = slot_frame_meta(slots[i]);

        EXPECT_EQ(ev->price(), golden[i].price) << "slot=" << i;
        EXPECT_EQ(ev->quantity(), golden[i].qty) << "slot=" << i;
        EXPECT_EQ(
            ev->side(),
            golden[i].is_bid ? Side_BUY : Side_SELL
        ) << "slot=" << i;

        EXPECT_EQ(meta.first_update_id, U) << "slot=" << i;
        EXPECT_EQ(meta.final_update_id, u) << "slot=" << i;

        // Re-inject the exact same slot into a second real SPSC path.
        ring_consume.push(slots[i]);
    }

    // Real L1 consumer + real LOB.
    auto lob_heap = std::make_unique<LobEngine>(P("81825.96"), kTick);
    LobEngine& lob = *lob_heap;
    L1Consumer consumer(ring_consume, gov, &lob);

    EXPECT_EQ(consumer.consume_available(), 36u);
    EXPECT_FALSE(consumer.is_degraded());

    // Verify representative exact levels after Consumer -> LOB.
    std::vector<LobEngine::Level> bids;
    std::vector<LobEngine::Level> asks;
    lob.get_book_levels(bids, asks, 64);

    ASSERT_EQ(bids.size(), 18u);
    ASSERT_EQ(asks.size(), 18u);

    // Bids are expected descending.
    for (std::size_t i = 0; i < 18; ++i) {
        EXPECT_EQ(bids[i].price, golden[i].price) << "bid index=" << i;
        EXPECT_EQ(bids[i].qty, golden[i].qty) << "bid index=" << i;
    }

    // LOB returns asks in descending index order.
    // The CLI reverses this sequence for human-readable ascending asks.
    for (std::size_t i = 0; i < 18; ++i) {
        const std::size_t g = 18 + (17 - i);
        EXPECT_EQ(asks[i].price, golden[g].price) << "ask index=" << i;
        EXPECT_EQ(asks[i].qty, golden[g].qty) << "ask index=" << i;
    }

    EXPECT_EQ(lob.best_bid_price(), P("81825.96"));
    EXPECT_EQ(lob.best_ask_price(), P("81825.97"));
    EXPECT_EQ(lob.best_bid_qty(), Q("2.54763"));
    EXPECT_EQ(lob.best_ask_qty(), Q("6.41168"));

    // Frame continuity is owned by L1Consumer in this pipeline.
    EXPECT_EQ(consumer.expected_next(), u + 1);
    EXPECT_FALSE(consumer.is_degraded());
}

} // namespace
