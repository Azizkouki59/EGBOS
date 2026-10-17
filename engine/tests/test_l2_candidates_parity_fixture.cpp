// engine/tests/test_l2_candidates_parity_fixture.cpp
// EGBOS — C++ fixture generator for offline Python byte-parity testing
//
// COMPUTABLE CANDIDATE — NOT AN EGBOS-APPROVED PRODUCTION FEATURE
// (see research-only quantitative reference — NOT EGBOS-SPECIFIED and current unresolved governance decision §FEAT-CONTRACT)
//
// This executable evaluates the computable candidates over a set of edge-case
// and random snapshot inputs, then serialises the results to a binary file.
// The Python parity runner (parity_runner.py) reads this file and asserts
// byte-identity (IEEE 754 float64 little-endian) against its own output.

#include "engine/l2/candidates/candidates_common.h"
#include "engine/l2/candidates/qty_features.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cmath>

using namespace egbos::l2::candidates;

// We need a NaN generator that produces a deterministic bit pattern
// across platforms if possible, though the Python side handles NaN
// by writing math.nan (which struct.pack encodes consistently on IEEE 754).
inline double make_nan() { return std::numeric_limits<double>::quiet_NaN(); }

// Helper to write raw bytes
template <typename T>
void write_val(std::ofstream& out, const T& val) {
    out.write(reinterpret_cast<const char*>(&val), sizeof(T));
}
void write_byte(std::ofstream& out, uint8_t val) {
    out.write(reinterpret_cast<const char*>(&val), 1);
}
void write_double(std::ofstream& out, const Result<double>& r) {
    if (r.ok()) {
        write_val(out, r.value);
        write_byte(out, 1);
    } else {
        write_val(out, make_nan());
        write_byte(out, 0);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output_binary_file>\n";
        return 1;
    }

    std::ofstream out(argv[1], std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file: " << argv[1] << "\n";
        return 1;
    }

    // Define test cases (QtyView inputs)
    std::vector<QtyView> cases;
    
    // Case 0: All zeros (denominator-zero triggers)
    cases.push_back(QtyView{});

    // Case 1: Balanced uniform
    {
        QtyView v{};
        for (unsigned i=0; i<kLevels; ++i) { v.bid_qtys[i] = 10; v.ask_qtys[i] = 10; }
        cases.push_back(v);
    }

    // Case 2: All bid
    {
        QtyView v{};
        for (unsigned i=0; i<kLevels; ++i) { v.bid_qtys[i] = 100; v.ask_qtys[i] = 0; }
        cases.push_back(v);
    }
    
    // Case 3: Mixed edge case
    {
        QtyView v{};
        v.bid_qtys[0] = 50; v.ask_qtys[0] = 50;
        v.bid_qtys[1] = 0;  v.ask_qtys[1] = 10;
        v.bid_qtys[2] = 0;  v.ask_qtys[2] = 0;
        v.bid_qtys[3] = 10; v.ask_qtys[3] = 0;
        v.bid_qtys[4] = 99; v.ask_qtys[4] = 99;
        cases.push_back(v);
    }

    // Write header
    uint32_t num_cases = cases.size();
    write_val(out, num_cases);

    // Process and write each case
    for (const auto& v : cases) {
        for (unsigned i=0; i<kLevels; ++i) write_val(out, v.bid_qtys[i]);
        for (unsigned i=0; i<kLevels; ++i) write_val(out, v.ask_qtys[i]);

        write_double(out, queue_imbalance_1_candidate(v));
        
        write_val(out, static_cast<uint64_t>(depth_bid_5_candidate(v)));
        write_val(out, static_cast<uint64_t>(depth_ask_5_candidate(v)));
        
        write_double(out, depth_imbalance_5_candidate(v));

        auto per_level = per_level_imbalance_candidate(v);
        for (const auto& r : per_level) write_double(out, r);

        auto cum = cumulative_depth_imbalance_candidate(v);
        for (const auto& r : cum) write_double(out, r);
    }

    out.close();
    std::cout << "Generated C++ parity fixture with " << num_cases << " cases to " << argv[1] << "\n";
    return 0;
}
