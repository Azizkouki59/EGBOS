// engine/core/snapshot_abi.h
#pragma once
#include <cstdint>
#include <cstddef>

namespace egbos::core {

static constexpr uint64_t kSnapshotMagic = 0x4547424F5353484DULL;
static constexpr uint32_t kFormatVersion = 1;
static constexpr uint32_t kSchemaVersion = 1;

#pragma pack(push, 1)

struct SnapshotHeader {
    uint64_t magic;
    uint32_t format_version;
    uint32_t schema_version;
    uint8_t  hash[32]; // Computed over the rest of the payload
};

struct SerializedPosition {
    int64_t position;
    uint64_t order_tag;
    int64_t x_l;
    int64_t gross_exposure;
};

struct SerializedRisk {
    uint32_t current_state;
};

struct SerializedLobHeader {
    int64_t anchor_price;
    int64_t tick_size;
    int32_t best_bid_index;
    int32_t best_ask_index;
    uint32_t order_count;
};

struct SerializedOrder {
    uint64_t order_id;
    uint32_t qty;
    int32_t price_index;
    uint8_t is_bid;
    uint8_t _pad[3]; // align to 8 bytes to avoid implicit padding
};

#pragma pack(pop)

static_assert(sizeof(SnapshotHeader) == 8 + 4 + 4 + 32, "SnapshotHeader size mismatch");
static_assert(sizeof(SerializedPosition) == 8 + 8 + 8 + 8, "SerializedPosition size mismatch");
static_assert(sizeof(SerializedRisk) == 4, "SerializedRisk size mismatch");
static_assert(sizeof(SerializedLobHeader) == 8 + 8 + 4 + 4 + 4, "SerializedLobHeader size mismatch");
static_assert(sizeof(SerializedOrder) == 8 + 4 + 4 + 4, "SerializedOrder size mismatch");

} // namespace egbos::core
