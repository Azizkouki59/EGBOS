#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace egbos::l1 {

// Binance production @depth stream can send up to 1000 price levels per message.
// Raise to 1024 (next power-of-2) to accommodate all production frames without
// ARRAY_TOO_LARGE parse failures.
constexpr std::size_t kMaxUpdatesPerPacket = 1024;
constexpr int64_t kPriceScale = 10000000LL; // 10^7 — see ADR R2: BTCUSDT-scoped, chosen for quantity precision headroom (stepSize 0.00001 has zero headroom at 10^5). Deliberately NOT matched to wal_writer.h, which serves a different purpose per ADR R2.

struct PriceQty {
    int64_t price;
    int64_t qty;
};
static_assert(sizeof(PriceQty) == 16, "PriceQty must be exactly 16 bytes — ADR R1 sizing depends on this");

struct ParsedDepthUpdate {
    uint64_t first_update_id;
    uint64_t final_update_id;
    std::array<PriceQty, kMaxUpdatesPerPacket> bids;
    std::size_t bid_count;
    std::array<PriceQty, kMaxUpdatesPerPacket> asks;
    std::size_t ask_count;
};

enum class ParseResult {
    OK,
    TRUNCATED,
    MISSING_FIELD,
    WRONG_EVENT_TYPE,
    NUMERIC_PARSE_FAILURE,
    SCIENTIFIC_NOTATION_REJECTED,
    PRECISION_EXCEEDED,
    OVERFLOW,
    WRONG_ARITY,
    ARRAY_TOO_LARGE,
    INVALID_UPDATE_ID_RANGE
};

ParseResult parse_depth_update(const uint8_t* data, size_t length, ParsedDepthUpdate* out);

} // namespace egbos::l1
