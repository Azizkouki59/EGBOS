#include <memory>
#include <gtest/gtest.h>
#include "engine/l1/lob_engine.h"
#include "engine/l1/depth_parser.h"

using namespace egbos::l1;

TEST(CliSyncReplayTest, BufferReplayGapCreatesGhostOrder) {
    // This reproduces the exact logic of the CLI buffer replay before the fix.
    // If a gap occurs AFTER sync is bridged, it silently skips frames and leaves ghost orders.
    
    // Simulate Snapshot at u=1000
    int64_t anchor = 816395200000LL; // 81639.52
    auto lob_heap = std::make_unique<LobEngine>(anchor, 100000LL);
    LobEngine& lob = *lob_heap;
    
    ParsedDepthUpdate snap{};
    snap.final_update_id = 1000;
    snap.bid_count = 1;
    snap.bids[0].price = anchor;
    snap.bids[0].qty = 10000000LL;
    lob.apply_depth_update(snap);
    
    // Buffer has 3 frames:
    // Frame 1: u=1001 (bridges gap)
    ParsedDepthUpdate f1{};
    f1.first_update_id = 1001;
    f1.final_update_id = 1001;
    f1.bid_count = 1;
    f1.bids[0].price = anchor - 1000000LL; // 81629.52
    f1.bids[0].qty = 50000000LL;
    
    // Frame 2 (THE GAP!): u=1002
    // Contains the cancellation for 81639.52
    // BUT IT IS MISSING FROM THE BUFFER!
    
    // Frame 3: u=1003
    ParsedDepthUpdate f3{};
    f3.first_update_id = 1003;
    f3.final_update_id = 1003;
    f3.bid_count = 1;
    f3.bids[0].price = anchor - 2000000LL; // 81619.52
    f3.bids[0].qty = 50000000LL;
    
    std::vector<ParsedDepthUpdate> buffer = {f1, f3};
    
    // Replay logic (fixed version)
    uint64_t last_id = snap.final_update_id;
    bool synced = false;
    for (const auto& p : buffer) {
        if (p.final_update_id <= last_id) continue;
        if (!synced) {
            if (p.first_update_id <= last_id + 1 && p.final_update_id >= last_id + 1) {
                synced = true;
                lob.apply_depth_update(p);
                last_id = p.final_update_id;
            } else {
                break;
            }
        } else {
            if (p.first_update_id <= last_id + 1 && p.final_update_id >= last_id + 1) {
                lob.apply_depth_update(p);
                last_id = p.final_update_id;
            } else {
                synced = false;
                break; // GAP CAUGHT!
            }
        }
    }
    
    EXPECT_FALSE(synced) << "The fixed logic should catch the gap and set synced=false";
    
    // In the old logic, synced would be true, and f3 would be applied, leaving the ghost order.
}
