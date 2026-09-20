#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include "engine/l1/lob_engine.h"

// S2-REQ-007: Libfuzzer Harness for LOB
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    // Need at least 21 bytes to construct order arguments safely
    if (Size < 24) return 0;
    
    auto engine_ptr = std::make_unique<egbos::l1::LobEngine>(100000000, 100);
    egbos::l1::LobEngine& engine = *engine_ptr;
    
    uint64_t order_id;
    int64_t price;
    uint32_t qty;
    bool is_bid;
    
    std::memcpy(&order_id, Data, 8);
    std::memcpy(&price, Data + 8, 8);
    std::memcpy(&qty, Data + 16, 4);
    is_bid = Data[20] % 2 == 0;
    
    // S2-REQ-005 mandates 0 allocations during this cycle
    engine.add_order(order_id, price, qty, is_bid);
    
    // Extract subsequent bits for modifications
    if (Size >= 28) {
        uint32_t mod_qty;
        std::memcpy(&mod_qty, Data + 24, 4);
        engine.modify_order(order_id, mod_qty);
    }
    
    engine.cancel_order(order_id);
    
    // Produce snapshot for fuzzing edge cases
    auto snap = engine.generate_top5_snapshot();
    (void)snap;
    
    return 0; // Return 0 to indicate valid fuzz execution (no crashes)
}
