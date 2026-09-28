// engine/l0/binance_json_normalizer.cpp
// EGBOS ENGINE_V3 — L0 Binance JSON Normalizer Implementation
//
// Specification  : S2-REQ-001 (Normalization Pipeline)
// Claims         : F1 (passthrough stub replaced), F2 (timestamp overwrite
//                  eliminated), F4 (PRICE_SCALE = 10^5)
//
// Zero dynamic allocation — all state is on the call stack.

#include "binance_json_normalizer.h"
#include <cstring>
#include <limits>

namespace egbos::l0 {

namespace {

// ── Helpers ───────────────────────────────────────────────────────────────────

inline void skip_ws(const uint8_t*& p, const uint8_t* end) noexcept {
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
        ++p;
}

// Returns pointer to next unescaped '"', or nullptr if not found.
inline const uint8_t* find_quote(const uint8_t* p, const uint8_t* end) noexcept {
    while (p < end) {
        if (*p == '"') return p;
        if (*p == '\\') ++p; // skip escaped char
        ++p;
    }
    return nullptr;
}

// Skip past a JSON value (any type), advancing p.
// Used to skip unknown keys.
L0ParseStatus skip_value(const uint8_t*& p, const uint8_t* end) noexcept {
    skip_ws(p, end);
    if (p == end) return L0ParseStatus::TRUNCATED;

    if (*p == '{') {
        ++p;
        int depth = 1;
        while (p < end && depth > 0) {
            if (*p == '"') {
                ++p;
                p = find_quote(p, end);
                if (!p) return L0ParseStatus::TRUNCATED;
            } else if (*p == '{') ++depth;
            else if (*p == '}') --depth;
            ++p;
        }
        return (depth == 0) ? L0ParseStatus::OK : L0ParseStatus::TRUNCATED;
    }
    if (*p == '[') {
        ++p;
        int depth = 1;
        while (p < end && depth > 0) {
            if (*p == '"') {
                ++p;
                p = find_quote(p, end);
                if (!p) return L0ParseStatus::TRUNCATED;
            } else if (*p == '[') ++depth;
            else if (*p == ']') --depth;
            ++p;
        }
        return (depth == 0) ? L0ParseStatus::OK : L0ParseStatus::TRUNCATED;
    }
    if (*p == '"') {
        ++p;
        p = find_quote(p, end);
        if (!p) return L0ParseStatus::TRUNCATED;
        ++p;
        return L0ParseStatus::OK;
    }
    // number / true / false / null
    while (p < end && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t')
        ++p;
    return L0ParseStatus::OK;
}

// Extract the contents of a JSON string value (without surrounding quotes).
// On success, [out_start, out_start+out_len) points into the source buffer.
L0ParseStatus extract_string(const uint8_t*& p, const uint8_t* end,
                              const uint8_t*& out_start,
                              std::size_t&    out_len) noexcept {
    skip_ws(p, end);
    if (p == end || *p != '"') return L0ParseStatus::TRUNCATED;
    ++p;
    const uint8_t* start = p;
    p = find_quote(p, end);
    if (!p) return L0ParseStatus::TRUNCATED;
    out_start = start;
    out_len   = static_cast<std::size_t>(p - start);
    ++p; // consume closing quote
    return L0ParseStatus::OK;
}

// Parse an unsigned decimal integer at [p, end).
L0ParseStatus parse_uint64(const uint8_t* p, const uint8_t* end,
                            uint64_t& out) noexcept {
    if (p == end) return L0ParseStatus::NUMERIC_PARSE_FAILURE;
    uint64_t val = 0;
    for (; p < end; ++p) {
        if (*p < '0' || *p > '9') return L0ParseStatus::NUMERIC_PARSE_FAILURE;
        uint64_t d = static_cast<uint64_t>(*p - '0');
        if (val > (std::numeric_limits<uint64_t>::max() - d) / 10)
            return L0ParseStatus::OVERFLOW;
        val = val * 10 + d;
    }
    out = val;
    return L0ParseStatus::OK;
}

// Parse a fixed-point decimal string using kL0PriceScale.
// Rejects scientific notation, excess precision, and overflow.
L0ParseStatus parse_fixed_point(const uint8_t* str, std::size_t len,
                                 int64_t& out_val) noexcept {
    if (len == 0) return L0ParseStatus::NUMERIC_PARSE_FAILURE;

    // Reject scientific notation.
    for (std::size_t i = 0; i < len; ++i) {
        if (str[i] == 'e' || str[i] == 'E')
            return L0ParseStatus::SCIENTIFIC_NOTATION;
    }

    const uint8_t* p   = str;
    const uint8_t* end = str + len;

    bool negative = false;
    if (p < end && *p == '-') { negative = true; ++p; }
    if (p == end) return L0ParseStatus::NUMERIC_PARSE_FAILURE;

    int64_t int_part = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        int64_t d = *p - '0';
        if (int_part > (std::numeric_limits<int64_t>::max() - d) / 10)
            return L0ParseStatus::OVERFLOW;
        int_part = int_part * 10 + d;
        ++p;
    }

    int64_t frac_part  = 0;
    int64_t frac_scale = 1;
    if (p < end && *p == '.') {
        ++p;
        while (p < end && *p >= '0' && *p <= '9') {
            const int64_t d = *p - '0';

            if (frac_scale < kL0PriceScale) {
                // Digits within the configured fixed-point precision.
                frac_part = frac_part * 10 + d;
                frac_scale *= 10;
            } else {
                // Accept exchange-side zero-padding beyond the fixed-point
                // precision, but never silently round a non-zero digit.
                if (d != 0)
                    return L0ParseStatus::PRECISION_EXCEEDED;
            }

            ++p;
        }
    }

    if (p != end) return L0ParseStatus::NUMERIC_PARSE_FAILURE; // trailing garbage

    // Scale up integer part
    if (int_part > 0 &&
        int_part > std::numeric_limits<int64_t>::max() / kL0PriceScale)
        return L0ParseStatus::OVERFLOW;
    int64_t result = int_part * kL0PriceScale;

    // Scale fractional part to match kL0PriceScale
    while (frac_scale < kL0PriceScale) {
        frac_part  *= 10;
        frac_scale *= 10;
    }

    if (result > std::numeric_limits<int64_t>::max() - frac_part)
        return L0ParseStatus::OVERFLOW;
    result += frac_part;

    out_val = negative ? -result : result;
    return L0ParseStatus::OK;
}

// Parse one bid/ask array ("b" or "a" JSON field) and invoke cb for each entry.
// side_enum selects Side_BUY or Side_SELL.
// event_type_enum selects EventType_NEW (non-zero qty) or EventType_CANCEL (qty==0).
L0ParseStatus parse_side_array(
    const uint8_t*& p,
    const uint8_t* end,
    egbos::events::Side side_enum,
    uint64_t  first_update_id,
    uint64_t  sequence_num,
    uint64_t  ingestion_tsc,
    std::size_t& level_count,
    L0EventCallback cb,
    void* user_ctx) noexcept
{
    skip_ws(p, end);
    if (p == end || *p != '[') return L0ParseStatus::TRUNCATED;
    ++p; // skip '['

    skip_ws(p, end);
    if (p < end && *p == ']') { ++p; return L0ParseStatus::OK; } // empty array

    while (p < end) {
        skip_ws(p, end);
        if (p == end || *p != '[') return L0ParseStatus::TRUNCATED;
        ++p; // skip inner '['

        // Detect empty inner array []
        if (p < end && *p == ']') return L0ParseStatus::WRONG_ARITY;

        const uint8_t* price_str; std::size_t price_len;
        L0ParseStatus r = extract_string(p, end, price_str, price_len);
        if (r != L0ParseStatus::OK) return r;

        skip_ws(p, end);
        if (p == end) return L0ParseStatus::TRUNCATED;
        if (*p == ']') return L0ParseStatus::WRONG_ARITY; // only 1 element
        if (*p != ',') return L0ParseStatus::TRUNCATED;
        ++p;

        const uint8_t* qty_str; std::size_t qty_len;
        r = extract_string(p, end, qty_str, qty_len);
        if (r != L0ParseStatus::OK) return r;

        skip_ws(p, end);
        if (p == end) return L0ParseStatus::TRUNCATED;
        if (*p == ',') return L0ParseStatus::WRONG_ARITY; // >2 elements
        if (*p != ']') return L0ParseStatus::TRUNCATED;
        ++p; // skip inner ']'

        if (level_count >= kMaxL0Levels) return L0ParseStatus::ARRAY_TOO_LARGE;

        int64_t price_val = 0;
        r = parse_fixed_point(price_str, price_len, price_val);
        if (r != L0ParseStatus::OK) return r;

        int64_t qty_val = 0;
        r = parse_fixed_point(qty_str, qty_len, qty_val);
        if (r != L0ParseStatus::OK) return r;

        // RawMarketEvent::quantity is uint32_t, but parse_fixed_point returns
        // an int64_t validated only against int64_t overflow (see
        // parse_fixed_point above). A value that fits in int64_t can still
        // exceed UINT32_MAX (max representable scaled quantity is
        // UINT32_MAX / kL0PriceScale = 42949.67 base-asset units). Silently
        // truncating via static_cast would wrap the value and misrepresent
        // the level's true quantity. Reject explicitly instead -- found and
        // documented in RFC-004 (engineering_findings.
        // raw_market_event_field_capacity_check); this guard applies to the
        // existing depth path and is not specific to trade events.
        if (qty_val > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
            return L0ParseStatus::OVERFLOW;
        }

        // EventType: CANCEL when qty==0 (Binance level removal), NEW otherwise.
        egbos::events::EventType ev_type = (qty_val == 0)
            ? egbos::events::EventType_CANCEL
            : egbos::events::EventType_NEW;

        // Construct RawMarketEvent directly (no heap).
        egbos::events::RawMarketEvent event(
            /*schema_version=*/ 1,
            /*event_type=*/      ev_type,
            /*side=*/            side_enum,
            /*feed_id=*/         0,
            /*exchange_id=*/     0,
            /*instrument_id=*/   0,
            /*quantity=*/        static_cast<uint32_t>(qty_val > 0 ? qty_val : 0),
            /*sequence_num=*/    sequence_num,
            /*ingestion_tsc=*/   ingestion_tsc,
            /*exchange_tsc=*/    0,           // not available in @depth stream
            /*price=*/           price_val,
            /*tier0_offset=*/    0,           // set by WAL writer
            /*checksum=*/        0            // not used at L0 push time
        );

        ++level_count;

        if (cb) {
            const uint32_t level_idx = static_cast<uint32_t>(level_count - 1);
            if (!cb(event, first_update_id, level_idx, user_ctx))
                return L0ParseStatus::OK; // caller aborted
        }

        skip_ws(p, end);
        if (p < end && *p == ',') {
            ++p;
        } else if (p < end && *p == ']') {
            ++p;
            break;
        } else {
            return L0ParseStatus::TRUNCATED;
        }
    }

    return L0ParseStatus::OK;
}

} // anonymous namespace

// ── normalize_binance_depth ───────────────────────────────────────────────────
// The Binance JSON object key order is not semantically significant.
// We therefore parse the frame twice:
//   Pass 1: validate and collect e/U/u before emitting anything.
//   Pass 2: parse b/a and emit levels with stable frame metadata.
// This remains zero-allocation and prevents U/u==0 when b/a precede U/u.
L0ParseStatus normalize_binance_depth(
    const uint8_t*  json,
    std::size_t     length,
    uint64_t        ingestion_tsc,
    L0EventCallback cb,
    void*           user_ctx) noexcept
{
    if (!json || length == 0) return L0ParseStatus::TRUNCATED;

    // ── Pass 1: collect frame metadata without emitting events ───────────────
    const uint8_t* p   = json;
    const uint8_t* end = json + length;

    skip_ws(p, end);
    if (p == end || *p != '{') return L0ParseStatus::TRUNCATED;
    ++p;

    uint32_t metadata_mask = 0;
    // bit 0: e, bit 1: u, bit 2: U
    static constexpr uint32_t kMetadataRequired = 0x07;

    uint64_t first_update_id = 0;
    uint64_t final_update_id = 0;

    while (p < end) {
        skip_ws(p, end);
        if (p == end) return L0ParseStatus::TRUNCATED;
        if (*p == '}') {
            ++p;
            break;
        }

        if (*p != '"') return L0ParseStatus::TRUNCATED;
        ++p;

        const uint8_t* key_start = p;
        p = find_quote(p, end);
        if (!p) return L0ParseStatus::TRUNCATED;

        const std::size_t key_len =
            static_cast<std::size_t>(p - key_start);
        ++p;

        skip_ws(p, end);
        if (p == end || *p != ':') return L0ParseStatus::TRUNCATED;
        ++p;
        skip_ws(p, end);

        if (key_len == 1) {
            const char k = static_cast<char>(key_start[0]);

            if (k == 'e') {
                const uint8_t* val;
                std::size_t val_len;

                L0ParseStatus r = extract_string(p, end, val, val_len);
                if (r != L0ParseStatus::OK) return r;

                if (val_len != 11 ||
                    std::memcmp(val, "depthUpdate", 11) != 0)
                    return L0ParseStatus::WRONG_EVENT_TYPE;

                metadata_mask |= 1;

            } else if (k == 'u') {
                const uint8_t* num_start = p;

                L0ParseStatus r = skip_value(p, end);
                if (r != L0ParseStatus::OK) return r;

                r = parse_uint64(num_start, p, final_update_id);
                if (r != L0ParseStatus::OK) return r;

                metadata_mask |= 2;

            } else if (k == 'U') {
                const uint8_t* num_start = p;

                L0ParseStatus r = skip_value(p, end);
                if (r != L0ParseStatus::OK) return r;

                r = parse_uint64(num_start, p, first_update_id);
                if (r != L0ParseStatus::OK) return r;

                metadata_mask |= 4;

            } else {
                L0ParseStatus r = skip_value(p, end);
                if (r != L0ParseStatus::OK) return r;
            }
        } else {
            L0ParseStatus r = skip_value(p, end);
            if (r != L0ParseStatus::OK) return r;
        }

        skip_ws(p, end);
        if (p < end && *p == ',') {
            ++p;
        } else if (p < end && *p == '}') {
            ++p;
            break;
        } else {
            return L0ParseStatus::TRUNCATED;
        }
    }

    if ((metadata_mask & kMetadataRequired) != kMetadataRequired)
        return L0ParseStatus::MISSING_FIELD;

    // Binance diff-depth continuity requires a valid update range.
    if (first_update_id > final_update_id)
        return L0ParseStatus::NUMERIC_PARSE_FAILURE;

    // ── Pass 2: parse b/a and emit only after U/u are known ───────────────────
    p = json;

    skip_ws(p, end);
    if (p == end || *p != '{') return L0ParseStatus::TRUNCATED;
    ++p;

    uint32_t found_mask = 0;
    // bit 0: e, bit 1: b, bit 2: a
    static constexpr uint32_t kRequiredOutputFields = 0x07;

    std::size_t level_count = 0;

    while (p < end) {
        skip_ws(p, end);
        if (p == end) return L0ParseStatus::TRUNCATED;
        if (*p == '}') {
            ++p;
            break;
        }

        if (*p != '"') return L0ParseStatus::TRUNCATED;
        ++p;

        const uint8_t* key_start = p;
        p = find_quote(p, end);
        if (!p) return L0ParseStatus::TRUNCATED;

        const std::size_t key_len =
            static_cast<std::size_t>(p - key_start);
        ++p;

        skip_ws(p, end);
        if (p == end || *p != ':') return L0ParseStatus::TRUNCATED;
        ++p;
        skip_ws(p, end);

        if (key_len == 1) {
            const char k = static_cast<char>(key_start[0]);

            if (k == 'e') {
                // Already validated in pass 1; consume it here as well.
                const uint8_t* val;
                std::size_t val_len;

                L0ParseStatus r = extract_string(p, end, val, val_len);
                if (r != L0ParseStatus::OK) return r;

                if (val_len != 11 ||
                    std::memcmp(val, "depthUpdate", 11) != 0)
                    return L0ParseStatus::WRONG_EVENT_TYPE;

                found_mask |= 1;

            } else if (k == 'b') {
                L0ParseStatus r = parse_side_array(
                    p, end,
                    egbos::events::Side_BUY,
                    first_update_id,
                    final_update_id,
                    ingestion_tsc,
                    level_count,
                    cb,
                    user_ctx);

                if (r != L0ParseStatus::OK) return r;
                found_mask |= 2;

            } else if (k == 'a') {
                L0ParseStatus r = parse_side_array(
                    p, end,
                    egbos::events::Side_SELL,
                    first_update_id,
                    final_update_id,
                    ingestion_tsc,
                    level_count,
                    cb,
                    user_ctx);

                if (r != L0ParseStatus::OK) return r;
                found_mask |= 4;

            } else {
                // U/u and all other fields were fully validated in pass 1.
                L0ParseStatus r = skip_value(p, end);
                if (r != L0ParseStatus::OK) return r;
            }
        } else {
            L0ParseStatus r = skip_value(p, end);
            if (r != L0ParseStatus::OK) return r;
        }

        skip_ws(p, end);
        if (p < end && *p == ',') {
            ++p;
        } else if (p < end && *p == '}') {
            ++p;
            break;
        } else {
            return L0ParseStatus::TRUNCATED;
        }
    }

    if ((found_mask & kRequiredOutputFields) != kRequiredOutputFields)
        return L0ParseStatus::MISSING_FIELD;

    return L0ParseStatus::OK;
}

namespace {

L0ParseStatus parse_trade_bool(
    const uint8_t*& p,
    const uint8_t* end,
    bool& out) noexcept
{
    skip_ws(p, end);

    if (static_cast<std::size_t>(end - p) >= 4 &&
        std::memcmp(p, "true", 4) == 0) {
        p += 4;
        out = true;
        return L0ParseStatus::OK;
    }

    if (static_cast<std::size_t>(end - p) >= 5 &&
        std::memcmp(p, "false", 5) == 0) {
        p += 5;
        out = false;
        return L0ParseStatus::OK;
    }

    return L0ParseStatus::NUMERIC_PARSE_FAILURE;
}

L0ParseStatus parse_trade_uint64(
    const uint8_t*& p,
    const uint8_t* end,
    uint64_t& out) noexcept
{
    const uint8_t* start = p;

    while (p < end &&
           *p != ',' &&
           *p != '}' &&
           *p != ' ' &&
           *p != '\n' &&
           *p != '\r' &&
           *p != '\t') {
        ++p;
    }

    if (p == start)
        return L0ParseStatus::NUMERIC_PARSE_FAILURE;

    return parse_uint64(start, p, out);
}

} // anonymous namespace

L0ParseStatus normalize_binance_trade(
    const uint8_t*       json,
    std::size_t          length,
    uint64_t             ingestion_tsc,
    L0TradeEventCallback cb,
    void*                user_ctx) noexcept
{
    if (!json || length == 0)
        return L0ParseStatus::TRUNCATED;

    const uint8_t* p = json;
    const uint8_t* end = json + length;

    skip_ws(p, end);
    if (p == end || *p != '{')
        return L0ParseStatus::TRUNCATED;
    ++p;

    bool have_e = false;
    bool have_s = false;
    const uint8_t* symbol_start = nullptr;
    std::size_t symbol_len = 0;
    bool have_t = false;
    bool have_p = false;
    bool have_q = false;
    bool have_T = false;
    bool have_m = false;

    uint64_t trade_id = 0;
    uint64_t trade_time_ms = 0;
    int64_t price_val = 0;
    int64_t qty_val = 0;
    bool buyer_is_maker = false;

    while (p < end) {
        skip_ws(p, end);

        if (p == end)
            return L0ParseStatus::TRUNCATED;

        if (*p == '}') {
            ++p;
            break;
        }

        if (*p != '"')
            return L0ParseStatus::TRUNCATED;
        ++p;

        const uint8_t* key_start = p;
        p = find_quote(p, end);
        if (!p)
            return L0ParseStatus::TRUNCATED;

        const std::size_t key_len =
            static_cast<std::size_t>(p - key_start);
        ++p;

        skip_ws(p, end);
        if (p == end || *p != ':')
            return L0ParseStatus::TRUNCATED;
        ++p;
        skip_ws(p, end);

        if (key_len == 1 && key_start[0] == 'e') {
            const uint8_t* value_start;
            std::size_t value_len;

            L0ParseStatus r =
                extract_string(p, end, value_start, value_len);
            if (r != L0ParseStatus::OK)
                return r;

            if (value_len != 5 ||
                std::memcmp(value_start, "trade", 5) != 0) {
                return L0ParseStatus::WRONG_EVENT_TYPE;
            }

            have_e = true;

        } else if (key_len == 1 && key_start[0] == 's') {
            const uint8_t* value_start;
            std::size_t value_len;

            L0ParseStatus r =
                extract_string(p, end, value_start, value_len);
            if (r != L0ParseStatus::OK)
                return r;

            if (value_len == 0)
                return L0ParseStatus::MISSING_FIELD;

            symbol_start = value_start;
            symbol_len = value_len;
            have_s = true;

        } else if (key_len == 1 && key_start[0] == 't') {
            L0ParseStatus r =
                parse_trade_uint64(p, end, trade_id);
            if (r != L0ParseStatus::OK)
                return r;

            have_t = true;

        } else if (key_len == 1 && key_start[0] == 'p') {
            const uint8_t* value_start;
            std::size_t value_len;

            L0ParseStatus r =
                extract_string(p, end, value_start, value_len);
            if (r != L0ParseStatus::OK)
                return r;

            r = parse_fixed_point(value_start, value_len, price_val);
            if (r != L0ParseStatus::OK)
                return r;

            if (price_val <= 0)
                return L0ParseStatus::NUMERIC_PARSE_FAILURE;

            have_p = true;

        } else if (key_len == 1 && key_start[0] == 'q') {
            const uint8_t* value_start;
            std::size_t value_len;

            L0ParseStatus r =
                extract_string(p, end, value_start, value_len);
            if (r != L0ParseStatus::OK)
                return r;

            r = parse_fixed_point(value_start, value_len, qty_val);
            if (r != L0ParseStatus::OK)
                return r;

            if (qty_val <= 0)
                return L0ParseStatus::NUMERIC_PARSE_FAILURE;

            if (qty_val >
                static_cast<int64_t>(
                    std::numeric_limits<uint32_t>::max())) {
                return L0ParseStatus::OVERFLOW;
            }

            have_q = true;

        } else if (key_len == 1 && key_start[0] == 'T') {
            L0ParseStatus r =
                parse_trade_uint64(p, end, trade_time_ms);
            if (r != L0ParseStatus::OK)
                return r;

            have_T = true;

        } else if (key_len == 1 && key_start[0] == 'm') {
            L0ParseStatus r =
                parse_trade_bool(p, end, buyer_is_maker);
            if (r != L0ParseStatus::OK)
                return r;

            have_m = true;

        } else {
            L0ParseStatus r = skip_value(p, end);
            if (r != L0ParseStatus::OK)
                return r;
        }

        skip_ws(p, end);

        if (p < end && *p == ',') {
            ++p;
        } else if (p < end && *p == '}') {
            ++p;
            break;
        } else {
            return L0ParseStatus::TRUNCATED;
        }
    }

    skip_ws(p, end);
    if (p != end)
        return L0ParseStatus::TRUNCATED;

    if (!have_e || !have_s || !have_t ||
        !have_p || !have_q || !have_T || !have_m) {
        return L0ParseStatus::MISSING_FIELD;
    }

    // RFC-004 current production scope is explicitly BTCUSDT -> instrument 0.
    // No authoritative symbol registry exists yet, so silently accepting
    // another symbol and assigning instrument_id=0 would misclassify it.
    if (symbol_len != 7 ||
        std::memcmp(symbol_start, "BTCUSDT", 7) != 0) {
        return L0ParseStatus::MISSING_FIELD;
    }

    constexpr uint64_t kMsToNs = 1'000'000ULL;

    if (trade_time_ms >
        std::numeric_limits<uint64_t>::max() / kMsToNs) {
        return L0ParseStatus::OVERFLOW;
    }

    const uint64_t exchange_tsc =
        trade_time_ms * kMsToNs;

    const egbos::events::Side side =
        buyer_is_maker
            ? egbos::events::Side_SELL
            : egbos::events::Side_BUY;

    const egbos::events::RawMarketEvent event(
        /*schema_version=*/ 1,
        /*event_type=*/      egbos::events::EventType_TRADE,
        /*side=*/            side,
        /*feed_id=*/         0,
        /*exchange_id=*/     0,
        /*instrument_id=*/   0,
        /*quantity=*/        static_cast<uint32_t>(qty_val),
        /*sequence_num=*/    trade_id,
        /*ingestion_tsc=*/   ingestion_tsc,
        /*exchange_tsc=*/    exchange_tsc,
        /*price=*/           price_val,
        /*tier0_offset=*/    0,
        /*checksum=*/        0);

    if (!cb)
        return L0ParseStatus::OK;

    cb(event, user_ctx);
    return L0ParseStatus::OK;
}

} // namespace egbos::l0
