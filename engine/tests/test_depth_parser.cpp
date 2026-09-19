// engine/tests/test_depth_parser.cpp
// EGBOS — depth_parser unit tests
//
// F4 fix: assertions updated for kPriceScale = 10^5 (100000).
// All price/qty expected values are recalculated accordingly.

#include <gtest/gtest.h>
#include "engine/l1/depth_parser.h"
#include <string>

using namespace egbos::l1;

// kPriceScale = 10000000 (10^7) as of R2 fix.
// Quick reference for the tests below:
//   "0.0024" × 10^7  =     24000
//   "0.0025" × 10^7  =     25000
//   "0.0026" × 10^7  =     26000
//   "10"     × 10^7  = 100000000
//   "11.5"   × 10^7  = 115000000
//   "100"    × 10^7  = 1000000000
//   "5"      × 10^7  =  50000000
//   "1"      × 10^7  =  10000000
//   "0.1"    × 10^7  =   1000000
//   "0.1234567" × 10^7 =   1234567  (exact 7 dp — OK)

TEST(DepthParserTest, ValidRoundTrip) {
    std::string json = R"({
        "e": "depthUpdate",
        "E": 1672515782136,
        "s": "BNBBTC",
        "U": 157,
        "u": 160,
        "b": [
            ["0.0024", "10"],
            ["0.0025", "11.5"]
        ],
        "a": [
            ["0.0026", "100"]
        ]
    })";

    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    ASSERT_EQ(result, ParseResult::OK);
    EXPECT_EQ(parsed.first_update_id, 157);
    EXPECT_EQ(parsed.final_update_id, 160);

    ASSERT_EQ(parsed.bid_count, 2);
    EXPECT_EQ(parsed.bids[0].price,      24000);       // "0.0024" × 10^7
    EXPECT_EQ(parsed.bids[0].qty,    100000000);       // "10"     × 10^7
    EXPECT_EQ(parsed.bids[1].price,      25000);       // "0.0025" × 10^7
    EXPECT_EQ(parsed.bids[1].qty,    115000000);       // "11.5"   × 10^7

    ASSERT_EQ(parsed.ask_count, 1);
    EXPECT_EQ(parsed.asks[0].price,      26000);       // "0.0026" × 10^7
    EXPECT_EQ(parsed.asks[0].qty,   1000000000);       // "100"    × 10^7
}

TEST(DepthParserTest, MissingField) {
    std::string json = R"({
        "e": "depthUpdate",
        "U": 157,
        "b": [],
        "a": []
    })"; // missing 'u'
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::MISSING_FIELD);
}

TEST(DepthParserTest, WrongEventType) {
    std::string json = R"({
        "e": "trade",
        "U": 157, "u": 160, "b": [], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::WRONG_EVENT_TYPE);
}

TEST(DepthParserTest, NumericParseFailure) {
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [["abc", "10"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::NUMERIC_PARSE_FAILURE);
}

TEST(DepthParserTest, ScientificNotationRejected) {
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [["1e-5", "10"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::SCIENTIFIC_NOTATION_REJECTED);
}

TEST(DepthParserTest, OverflowPrice) {
    // kPriceScale = 10^5.  INT64_MAX / 10^5 = 92233720368547.
    // Any integer part > 92233720368547 overflows when multiplied by scale.
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [["92233720368548", "10"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::OVERFLOW);
}

TEST(DepthParserTest, WrongArity) {
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [["0.01"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::WRONG_ARITY);
}

TEST(DepthParserTest, ArrayArityTooLarge) {
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [["0.01", "1", "2"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::WRONG_ARITY);
}

TEST(DepthParserTest, EmptyInnerArray) {
    // 0-element inner array `[]` inside "b"
    // Justification: The ADR requires array entry arity to be exactly 2 ([price, qty]).
    // An empty array has arity 0, which is not 2. Therefore, it is WRONG_ARITY.
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [[]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::WRONG_ARITY);
}

TEST(DepthParserTest, ExactlyMaxUpdatesPerPacket) {
    const std::size_t N = kMaxUpdatesPerPacket;
    std::string json = "{\"e\":\"depthUpdate\",\"U\":1,\"u\":2,\"b\":[";
    for (std::size_t i = 0; i < N; ++i) {
        json += "[\"1\",\"1\"]";
        if (i < N - 1) json += ",";
    }
    json += "],\"a\":[]}";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::OK);
    EXPECT_EQ(parsed.bid_count, N);
}

TEST(DepthParserTest, ArrayTooLarge) {
    const std::size_t N = kMaxUpdatesPerPacket + 1;
    std::string json = "{\"e\":\"depthUpdate\",\"U\":1,\"u\":2,\"b\":[";
    for (std::size_t i = 0; i < N; ++i) {
        json += "[\"1\",\"1\"]";
        if (i < N - 1) json += ",";
    }
    json += "],\"a\":[]}";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::ARRAY_TOO_LARGE);
}

TEST(DepthParserTest, ZeroEntryArrays) {
    std::string json = R"({
        "e": "depthUpdate", "U": 1, "u": 2, "b": [], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    ASSERT_EQ(result, ParseResult::OK);
    EXPECT_EQ(parsed.bid_count, 0);
    EXPECT_EQ(parsed.ask_count, 0);
}

TEST(DepthParserTest, MixedZeroAndNonZeroQty) {
    std::string json = R"({
        "e": "depthUpdate", "U": 1, "u": 2,
        "b": [["1.0", "0"], ["2.0", "5"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    ASSERT_EQ(result, ParseResult::OK);
    EXPECT_EQ(parsed.bid_count, 2);
    EXPECT_EQ(parsed.bids[0].qty, 0);
    EXPECT_EQ(parsed.bids[1].qty, 50000000);  // "5" × 10^7 = 50000000
}

TEST(DepthParserTest, TruncatedAt128Bytes) {
    std::string json = R"({"e":"depthUpdate","E":1672515782136,"s":"BNBBTC","U":157,"u":160,"b":[["0.0024","10"]],"a":[["0.0026","100"],["0.0027","200"],["0.0028","300"]]})";
    ASSERT_GT(json.size(), 128); // Ensure it's larger than 128
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), 128, &parsed);
    EXPECT_EQ(result, ParseResult::TRUNCATED);
}

TEST(DepthParserTest, InvalidUpdateIdRange) {
    std::string json = R"({
        "e": "depthUpdate", "U": 160, "u": 157, "b": [], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::INVALID_UPDATE_ID_RANGE);
}

TEST(DepthParserTest, ExactPrecision) {
    // kPriceScale = 10^7 supports exactly 7 decimal places.
    // "0.1234567" has 7 dp → exact, no PRECISION_EXCEEDED.
    // result = 1234567
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [["0.1234567", "10"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    ASSERT_EQ(result, ParseResult::OK);
    EXPECT_EQ(parsed.bids[0].price, 1234567);  // "0.1234567" × 10^7
}

TEST(DepthParserTest, ExcessPrecision) {
    // kPriceScale = 10^7 supports exactly 7 decimal places.
    // "0.12345678" has 8 dp with a non-zero 8th digit → PRECISION_EXCEEDED.
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [["0.12345678", "10"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    EXPECT_EQ(result, ParseResult::PRECISION_EXCEEDED);
}

TEST(DepthParserTest, TrailingZeroPadding) {
    // Binance always emits prices with 8 decimal places padded with trailing
    // zeros, e.g. "76721.73000000".  With kPriceScale = 10^7 (7 dp), the
    // trailing zero digit must be accepted silently — not rejected as
    // PRECISION_EXCEEDED.
    // "76721.73000000" × 10^7 = 767217300000
    std::string json = R"({
        "e": "depthUpdate", "U": 157, "u": 160,
        "b": [["76721.73000000", "0.06498000"]], "a": []
    })";
    ParsedDepthUpdate parsed;
    ParseResult result = parse_depth_update(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &parsed);
    ASSERT_EQ(result, ParseResult::OK);
    EXPECT_EQ(parsed.bids[0].price, 767217300000LL);  // 76721.73 × 10^7
    EXPECT_EQ(parsed.bids[0].qty,        649800LL);   // 0.06498 × 10^7
}
