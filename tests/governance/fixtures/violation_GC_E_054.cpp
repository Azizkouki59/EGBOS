// tests/governance/fixtures/violation_GC_E_054.cpp
// §15.4 Violation Fixture: GC-E-054 (research SIMD flags non-compliant)
//
// This file intentionally uses SIMD intrinsics outside the approved
// research/simd/ directory without the required govcheck exemption
// annotation. govcheck source scanning must detect this and fire GC-E-054.
//
// Pre-registered — the rule must exist and fire even though the full
// research SIMD audit is a later phase.
//
// Expected: govcheck reports GC-E-054 on this file.

#include <immintrin.h>

namespace egbos::research {

// VIOLATION: SIMD intrinsics used outside research/simd/ approved path
// without GC-E-054 exemption annotation.
void non_compliant_simd_usage(float* data, int n) {
    // GC-E-054: research SIMD flags non-compliant
    __m256 vec = _mm256_loadu_ps(data);
    __m256 result = _mm256_add_ps(vec, vec);
    _mm256_storeu_ps(data, result);
}

}  // namespace egbos::research
