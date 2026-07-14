// tests/governance/fixtures/violation_GC_E_010.cpp
// §15.4 Violation Fixture: GC-E-010 (malloc/new in engine/)
//
// This file intentionally contains a malloc call inside engine/ code.
// govcheck source scanning must detect this and fire GC-E-010.
// If this fixture passes CI without triggering GC-E-010, the rule is broken.
//
// Expected: govcheck reports GC-E-010 (Warning) on this file.

#include <cstdlib>

namespace egbos::core {

// VIOLATION: malloc in engine/ — forbidden by GC-E-010
void* allocate_from_heap(std::size_t n) {
    return malloc(n);  // GC-E-010: malloc in engine/
}

}  // namespace egbos::core
