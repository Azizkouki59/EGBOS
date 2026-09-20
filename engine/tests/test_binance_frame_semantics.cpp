// engine/tests/test_binance_frame_semantics.cpp
// Deterministic tests for Binance U/u frame sequence semantics.
// Classification: DEVELOPMENT_TEST

#include <gtest/gtest.h>
#include "engine/l1/depth_parser.h"
#include "engine/l1/lob_engine.h"

using namespace egbos::l1;
using namespace egbos::core;

class BinanceFrameSemanticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        lob = std::make_unique<LobEngine>(100000LL, 100LL);
    }
    std::unique_ptr<LobEngine> lob;
};

// Test A & D: Same-frame multi-level + no false intra-frame gap
TEST_F(BinanceFrameSemanticsTest, SameFrameMultiLevel) {
    ParsedDepthUpdate update{};
    update.first_update_id = 100;
    update.final_update_id = 100;
    
    update.bids[0] = {100000LL, 500LL};
    update.bids[1] = {99000LL,  300LL};
    update.bid_count = 2;
    
    update.asks[0] = {101000LL, 200LL};
    update.ask_count = 1;
    
    lob->apply_depth_update(update);
    
    EXPECT_EQ(lob->last_update_id(), 100);
    EXPECT_EQ(lob->depth_frames_applied(), 1);
    EXPECT_EQ(lob->levels_applied(), 3);
    
    EXPECT_EQ(lob->best_bid_price(), 100000LL);
    EXPECT_EQ(lob->best_bid_qty(), 500LL);
    EXPECT_EQ(lob->best_ask_price(), 101000LL);
}

// Test B: Valid continuation
TEST_F(BinanceFrameSemanticsTest, ValidContinuation) {
    ParsedDepthUpdate u1{};
    u1.first_update_id = 100;
    u1.final_update_id = 100;
    u1.bids[0] = {100000LL, 500LL};
    u1.bid_count = 1;
    lob->apply_depth_update(u1);
    
    ParsedDepthUpdate u2{};
    u2.first_update_id = 101;
    u2.final_update_id = 105;
    u2.bids[0] = {100000LL, 600LL};
    u2.bid_count = 1;
    lob->apply_depth_update(u2);
    
    EXPECT_EQ(lob->last_update_id(), 105);
    EXPECT_EQ(lob->best_bid_qty(), 600LL);
}

// Test F & G: Zero-qty deletion and new level insertion
TEST_F(BinanceFrameSemanticsTest, ZeroQuantityDeletion) {
    ParsedDepthUpdate u1{};
    u1.first_update_id = 100;
    u1.final_update_id = 100;
    u1.bids[0] = {100000LL, 500LL};
    u1.bid_count = 1;
    lob->apply_depth_update(u1);
    
    ParsedDepthUpdate u2{};
    u2.first_update_id = 101;
    u2.final_update_id = 101;
    u2.bids[0] = {100000LL, 0LL}; // Deletion
    u2.bid_count = 1;
    lob->apply_depth_update(u2);
    
    EXPECT_EQ(lob->best_bid_price(), 0); // Empty
}

// Test H: BBO Consistency
TEST_F(BinanceFrameSemanticsTest, BboConsistency) {
    ParsedDepthUpdate u1{};
    u1.first_update_id = 100;
    u1.final_update_id = 100;
    u1.bids[0] = {100000LL, 500LL};
    u1.asks[0] = {101000LL, 200LL};
    u1.bid_count = 1;
    u1.ask_count = 1;
    lob->apply_depth_update(u1);
    
    EXPECT_EQ(lob->best_bid_price(), 100000LL);
    EXPECT_EQ(lob->best_ask_price(), 101000LL);
    
    // Cross over BBO (invalid in real market, but engine should resolve it based on sequence)
    ParsedDepthUpdate u2{};
    u2.first_update_id = 101;
    u2.final_update_id = 101;
    u2.asks[0] = {101000LL, 0LL}; // delete ask
    u2.asks[1] = {99500LL, 100LL}; // new ask below bid
    u2.ask_count = 2;
    lob->apply_depth_update(u2);
    
    EXPECT_EQ(lob->best_ask_price(), 99500LL); // Engine blindly updates
}
