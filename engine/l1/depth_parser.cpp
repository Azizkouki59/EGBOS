#include "depth_parser.h"
#include <cstring>
#include <limits>
#include <charconv>

namespace egbos::l1 {

namespace {

// Skip whitespace
void skip_whitespace(const uint8_t*& p, const uint8_t* end) {
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) {
        p++;
    }
}

// Fixed-point parsing
ParseResult parse_fixed_point(const uint8_t* str, size_t len, int64_t scale, int64_t& out_val) {
    if (len == 0) return ParseResult::NUMERIC_PARSE_FAILURE;

    // Check for scientific notation
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == 'e' || str[i] == 'E') {
            return ParseResult::SCIENTIFIC_NOTATION_REJECTED;
        }
    }

    const uint8_t* p = str;
    const uint8_t* end = str + len;

    bool negative = false;
    if (p < end && *p == '-') {
        negative = true;
        p++;
    }
    if (p == end) return ParseResult::NUMERIC_PARSE_FAILURE;

    int64_t int_part = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        int64_t digit = *p - '0';
        // Check overflow for int_part * 10 + digit
        if (int_part > (std::numeric_limits<int64_t>::max() - digit) / 10) {
            return ParseResult::OVERFLOW;
        }
        int_part = int_part * 10 + digit;
        p++;
    }

    int64_t frac_part = 0;
    int64_t frac_scale = 1;
    if (p < end && *p == '.') {
        p++;
        while (p < end && *p >= '0' && *p <= '9') {
            if (frac_scale < scale) {
                int64_t digit = *p - '0';
                frac_part = frac_part * 10 + digit;
                frac_scale *= 10;
            } else {
                // Scale budget exhausted.  Trailing zeros are harmless padding
                // (Binance always emits 8 dp regardless of kPriceScale).
                // A non-zero digit genuinely exceeds the representable precision.
                if (*p != '0') {
                    return ParseResult::PRECISION_EXCEEDED;
                }
                // zero — consume silently and continue
            }
            p++;
        }
    }

    if (p != end) return ParseResult::NUMERIC_PARSE_FAILURE; // trailing garbage

    // Scale up the integer part
    // Check overflow for int_part * scale
    if (int_part > 0 && int_part > std::numeric_limits<int64_t>::max() / scale) {
        return ParseResult::OVERFLOW;
    }
    int64_t result = int_part * scale;

    // Scale the fractional part to match the requested scale
    while (frac_scale < scale) {
        frac_part *= 10;
        frac_scale *= 10;
    }

    if (result > std::numeric_limits<int64_t>::max() - frac_part) {
        return ParseResult::OVERFLOW;
    }
    result += frac_part;

    out_val = negative ? -result : result;
    return ParseResult::OK;
}

// Parse unsigned integer
ParseResult parse_uint64(const uint8_t* p, const uint8_t* end, uint64_t& out_val) {
    if (p == end) return ParseResult::NUMERIC_PARSE_FAILURE;
    uint64_t val = 0;
    for (; p < end; ++p) {
        if (*p < '0' || *p > '9') return ParseResult::NUMERIC_PARSE_FAILURE;
        uint64_t digit = *p - '0';
        if (val > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            return ParseResult::OVERFLOW;
        }
        val = val * 10 + digit;
    }
    out_val = val;
    return ParseResult::OK;
}

// Match string exactly
bool match_string(const uint8_t*& p, const uint8_t* end, const char* target) {
    size_t len = std::strlen(target);
    if (end - p < static_cast<ptrdiff_t>(len)) return false;
    if (std::memcmp(p, target, len) == 0) {
        p += len;
        return true;
    }
    return false;
}

// Find next unescaped quote
const uint8_t* find_quote(const uint8_t* p, const uint8_t* end) {
    while (p < end) {
        if (*p == '"') return p;
        if (*p == '\\') {
            p++; // skip escaped char
        }
        p++;
    }
    return nullptr;
}

// Skip JSON value
ParseResult skip_value(const uint8_t*& p, const uint8_t* end) {
    skip_whitespace(p, end);
    if (p == end) return ParseResult::TRUNCATED;

    if (*p == '{') {
        p++;
        int depth = 1;
        while (p < end && depth > 0) {
            if (*p == '"') {
                p++;
                p = find_quote(p, end);
                if (!p) return ParseResult::TRUNCATED;
            } else if (*p == '{') depth++;
            else if (*p == '}') depth--;
            p++;
        }
        if (depth > 0) return ParseResult::TRUNCATED;
    } else if (*p == '[') {
        p++;
        int depth = 1;
        while (p < end && depth > 0) {
            if (*p == '"') {
                p++;
                p = find_quote(p, end);
                if (!p) return ParseResult::TRUNCATED;
            } else if (*p == '[') depth++;
            else if (*p == ']') depth--;
            p++;
        }
        if (depth > 0) return ParseResult::TRUNCATED;
    } else if (*p == '"') {
        p++;
        p = find_quote(p, end);
        if (!p) return ParseResult::TRUNCATED;
        p++;
    } else {
        // number, true, false, null
        while (p < end && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
            p++;
        }
    }
    return ParseResult::OK;
}

ParseResult extract_string(const uint8_t*& p, const uint8_t* end, const uint8_t*& out_str, size_t& out_len) {
    skip_whitespace(p, end);
    if (p == end || *p != '"') return ParseResult::TRUNCATED;
    p++;
    const uint8_t* start = p;
    p = find_quote(p, end);
    if (!p) return ParseResult::TRUNCATED;
    out_str = start;
    out_len = p - start;
    p++; // skip closing quote
    return ParseResult::OK;
}

ParseResult parse_price_qty_array(const uint8_t*& p, const uint8_t* end, std::array<PriceQty, kMaxUpdatesPerPacket>& arr, size_t& count) {
    skip_whitespace(p, end);
    if (p == end || *p != '[') return ParseResult::TRUNCATED;
    p++; // skip '['
    
    count = 0;
    skip_whitespace(p, end);
    
    if (p < end && *p == ']') {
        p++;
        return ParseResult::OK; // Empty array
    }

    while (p < end) {
        skip_whitespace(p, end);
        if (p == end || *p != '[') return ParseResult::TRUNCATED;
        p++;

        const uint8_t* price_str;
        size_t price_len;
        // Detect an empty inner array []
        if (p < end && *p == ']') return ParseResult::WRONG_ARITY;

        ParseResult r = extract_string(p, end, price_str, price_len);
        if (r != ParseResult::OK) return r;

        skip_whitespace(p, end);
        if (p == end) return ParseResult::TRUNCATED;
        if (*p == ']') return ParseResult::WRONG_ARITY; // Only 1 element
        if (*p != ',') return ParseResult::TRUNCATED;
        p++;

        const uint8_t* qty_str;
        size_t qty_len;
        r = extract_string(p, end, qty_str, qty_len);
        if (r != ParseResult::OK) return r;

        skip_whitespace(p, end);
        if (p == end) return ParseResult::TRUNCATED;
        if (*p == ',') return ParseResult::WRONG_ARITY; // More than 2 elements
        if (*p != ']') return ParseResult::TRUNCATED;
        p++;

        if (count >= kMaxUpdatesPerPacket) {
            return ParseResult::ARRAY_TOO_LARGE;
        }

        int64_t price_val = 0;
        int64_t qty_val = 0;
        r = parse_fixed_point(price_str, price_len, kPriceScale, price_val);
        if (r != ParseResult::OK) return r;
        r = parse_fixed_point(qty_str, qty_len, kPriceScale, qty_val);
        if (r != ParseResult::OK) return r;

        // S6-REQ-002: Deterministic parser-level bounds rejection for LOB quantities.
        if (qty_val < 0 || qty_val > 0xFFFFFFFFLL) {
            return ParseResult::OVERFLOW;
        }

        arr[count].price = price_val;
        arr[count].qty = qty_val;
        count++;

        skip_whitespace(p, end);
        if (p < end && *p == ',') {
            p++;
        } else if (p < end && *p == ']') {
            p++;
            break;
        } else {
            return ParseResult::TRUNCATED;
        }
    }
    
    return ParseResult::OK;
}

} // namespace

ParseResult parse_depth_update(const uint8_t* data, size_t length, ParsedDepthUpdate* out) {
    if (!data || !out || length == 0) return ParseResult::TRUNCATED;

    ParsedDepthUpdate temp;
    temp.first_update_id = 0;
    temp.final_update_id = 0;
    temp.bid_count = 0;
    temp.ask_count = 0;

    uint32_t found_mask = 0; // bit 0: e, bit 1: U, bit 2: u, bit 3: b, bit 4: a
    const uint32_t ALL_FOUND = 0x1F;

    const uint8_t* p = data;
    const uint8_t* end = data + length;

    skip_whitespace(p, end);
    if (p == end || *p != '{') return ParseResult::TRUNCATED;
    p++; // skip '{'

    while (p < end) {
        skip_whitespace(p, end);
        if (p == end) return ParseResult::TRUNCATED;
        if (*p == '}') {
            p++;
            break; // End of object
        }

        // Must be a string key
        if (*p != '"') return ParseResult::TRUNCATED;
        p++;
        const uint8_t* key_start = p;
        p = find_quote(p, end);
        if (!p) return ParseResult::TRUNCATED;
        size_t key_len = p - key_start;
        p++; // skip closing quote

        skip_whitespace(p, end);
        if (p == end || *p != ':') return ParseResult::TRUNCATED;
        p++; // skip ':'
        
        skip_whitespace(p, end);

        if (key_len == 1) {
            char k = key_start[0];
            if (k == 'e') {
                const uint8_t* val_str;
                size_t val_len;
                ParseResult r = extract_string(p, end, val_str, val_len);
                if (r != ParseResult::OK) return r;
                if (val_len != 11 || std::memcmp(val_str, "depthUpdate", 11) != 0) {
                    return ParseResult::WRONG_EVENT_TYPE;
                }
                found_mask |= 1;
            } else if (k == 'U') {
                // Bare integer
                const uint8_t* num_start = p;
                ParseResult r = skip_value(p, end);
                if (r != ParseResult::OK) return r;
                r = parse_uint64(num_start, p, temp.first_update_id);
                if (r != ParseResult::OK) return r;
                found_mask |= 2;
            } else if (k == 'u') {
                const uint8_t* num_start = p;
                ParseResult r = skip_value(p, end);
                if (r != ParseResult::OK) return r;
                r = parse_uint64(num_start, p, temp.final_update_id);
                if (r != ParseResult::OK) return r;
                found_mask |= 4;
            } else if (k == 'b') {
                ParseResult r = parse_price_qty_array(p, end, temp.bids, temp.bid_count);
                if (r != ParseResult::OK) return r;
                found_mask |= 8;
            } else if (k == 'a') {
                ParseResult r = parse_price_qty_array(p, end, temp.asks, temp.ask_count);
                if (r != ParseResult::OK) return r;
                found_mask |= 16;
            } else {
                ParseResult r = skip_value(p, end);
                if (r != ParseResult::OK) return r;
            }
        } else {
            ParseResult r = skip_value(p, end);
            if (r != ParseResult::OK) return r;
        }

        skip_whitespace(p, end);
        if (p < end && *p == ',') {
            p++;
        } else if (p < end && *p == '}') {
            p++;
            break;
        } else {
            return ParseResult::TRUNCATED;
        }
    }

    if (found_mask != ALL_FOUND) {
        return ParseResult::MISSING_FIELD;
    }

    if (temp.first_update_id > temp.final_update_id) {
        return ParseResult::INVALID_UPDATE_ID_RANGE;
    }

    *out = temp;
    return ParseResult::OK;
}

} // namespace egbos::l1
