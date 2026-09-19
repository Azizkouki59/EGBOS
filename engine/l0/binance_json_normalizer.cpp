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
            if (frac_scale < kL0PriceScale) {
                int64_t d = *p - '0';
                frac_part = frac_part * 10 + d;
                frac_scale *= 10;
            } else {
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
L0ParseStatus normalize_binance_depth(
    const uint8_t*  json,
    std::size_t     length,
    uint64_t        ingestion_tsc,
    L0EventCallback cb,
    void*           user_ctx) noexcept
{
    if (!json || length == 0) return L0ParseStatus::TRUNCATED;

    const uint8_t* p   = json;
    const uint8_t* end = json + length;

    skip_ws(p, end);
    if (p == end || *p != '{') return L0ParseStatus::TRUNCATED;
    ++p; // skip '{'

    // Fields we must find: e (event type), u (final_update_id), b, a.
    // U (first_update_id) is read but not used — gap detection is deferred.
    uint32_t found_mask = 0;
    // bit 0: e, bit 1: u, bit 2: b, bit 3: a
    static constexpr uint32_t kAllRequired = 0x0F;

    uint64_t first_update_id = 0;
    uint64_t final_update_id = 0;
    std::size_t level_count  = 0;

    while (p < end) {
        skip_ws(p, end);
        if (p == end) return L0ParseStatus::TRUNCATED;
        if (*p == '}') { ++p; break; }

        // Key must start with '"'
        if (*p != '"') return L0ParseStatus::TRUNCATED;
        ++p;
        const uint8_t* key_start = p;
        p = find_quote(p, end);
        if (!p) return L0ParseStatus::TRUNCATED;
        std::size_t key_len = static_cast<std::size_t>(p - key_start);
        ++p; // skip closing quote

        skip_ws(p, end);
        if (p == end || *p != ':') return L0ParseStatus::TRUNCATED;
        ++p;
        skip_ws(p, end);

        if (key_len == 1) {
            char k = static_cast<char>(key_start[0]);

            if (k == 'e') {
                // Value must be "depthUpdate"
                const uint8_t* val; std::size_t val_len;
                L0ParseStatus r = extract_string(p, end, val, val_len);
                if (r != L0ParseStatus::OK) return r;
                if (val_len != 11 || std::memcmp(val, "depthUpdate", 11) != 0)
                    return L0ParseStatus::WRONG_EVENT_TYPE;
                found_mask |= 1;

            } else if (k == 'u') {
                // final_update_id (bare integer)
                const uint8_t* num_start = p;
                L0ParseStatus r = skip_value(p, end);
                if (r != L0ParseStatus::OK) return r;
                r = parse_uint64(num_start, p, final_update_id);
                if (r != L0ParseStatus::OK) return r;
                found_mask |= 2;

            } else if (k == 'U') {
                // first_update_id — used for frame-level gap detection in L1
                const uint8_t* num_start = p;
                L0ParseStatus r = skip_value(p, end);
                if (r != L0ParseStatus::OK) return r;
                r = parse_uint64(num_start, p, first_update_id);
                if (r != L0ParseStatus::OK) return r;

            } else if (k == 'b') {
                L0ParseStatus r = parse_side_array(
                    p, end,
                    egbos::events::Side_BUY,
                    /*first_update_id=*/  first_update_id,
                    /*sequence_num=*/     final_update_id, // may be 0 if 'u' not yet seen
                    ingestion_tsc,
                    level_count, cb, user_ctx);
                if (r != L0ParseStatus::OK) return r;
                found_mask |= 4;

            } else if (k == 'a') {
                L0ParseStatus r = parse_side_array(
                    p, end,
                    egbos::events::Side_SELL,
                    /*first_update_id=*/  first_update_id,
                    /*sequence_num=*/     final_update_id,
                    ingestion_tsc,
                    level_count, cb, user_ctx);
                if (r != L0ParseStatus::OK) return r;
                found_mask |= 8;

            } else {
                L0ParseStatus r = skip_value(p, end);
                if (r != L0ParseStatus::OK) return r;
            }
        } else {
            // Multi-char key (E, s, pu, …) — skip value
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

    if ((found_mask & kAllRequired) != kAllRequired)
        return L0ParseStatus::MISSING_FIELD;

    return L0ParseStatus::OK;
}

} // namespace egbos::l0
