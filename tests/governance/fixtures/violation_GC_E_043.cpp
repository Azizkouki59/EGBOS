// tests/governance/fixtures/violation_GC_E_043.cpp
// §15.4 Violation Fixture: GC-E-043 (L3 state encapsulation)
//
// This file simulates external code that directly accesses `order_tag_` or
// `x_l_` without using the CROSS_PLANE_READ macro. govcheck source scanning
// must detect this and fire GC-E-043.
//
// GC-E-043 applies to code outside position_engine.cpp / position_engine.h.
// Any reference to `order_tag_` or `x_l_` in non-L3 sources without
// CROSS_PLANE_READ is a blocking violation.
//
// Expected: govcheck reports GC-E-043 on this file.

#include <cstdint>

namespace egbos::external_violation {

int64_t read_tag_VIOLATION() {
    // GC-E-043: direct external access to L3-private state
    int64_t order_tag_ = 0;  // not inside position_engine — violation
    return order_tag_;
}

}  // namespace egbos::external_violation
