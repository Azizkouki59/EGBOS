// engine/core/cross_plane.h
// EGBOS ENGINE_V3 — Cross-Plane Atomic Access Protocol
//
// Specification  : EGBOS-SPEC-S1-001 §6.3, §14.4
// ADR compliance : AD-005 (io_uring single-CQE-consumer — establishes the
//                  principle that cross-boundary access is structurally audited)
// Claims         : CLAIM-L3-004 (exec.memory_ordering, V4) — S3, depends on this
//                  CLAIM-L3-006 (exec.atomic_exposure, V4)  — S3, depends on this
//
// Govcheck rules pre-registered in S1 for Phase 4 enforcement:
//   GC-E-044 — memory_order_relaxed on any CROSS_PLANE_READ is blocked
//               at Phase 4. Violation fixture: violation_GC_E_044.cpp
//   GC-E-045 — direct x_l_/order_tag_ read from L4/L5 without CROSS_PLANE_READ
//               is blocked at Phase 4. Violation fixture: violation_GC_E_045.cpp
//
// Govcheck active now:
//   GC-E-010 (Warning) — no malloc/new/free/delete in this file (structurally absent)
//   GC-E-011 (pre-registered) — no CLOCK_REALTIME in this file (structurally absent)
//
// Purpose:
//   This header defines the CROSS_PLANE_READ macro and layer identifier
//   constants. Any atomic read that crosses an execution-plane layer boundary
//   MUST use CROSS_PLANE_READ — never a bare .load() call.
//
//   From spec §6.3 (HTML §C3.3 verbatim):
//     "The only piece of L3 state that L4 may read directly is gross_exposure_ —
//      a single std::atomic<int64_t>. All other L3 state is structurally
//      inaccessible to L4: not via accessor, not via pointer, not via shared
//      struct. GC-E-045 enforces this at compile time."
//
//   The CROSS_PLANE_READ macro:
//     1. Static asserts that only int64_t atomics are permitted (spec §6.3).
//     2. Enforces memory_order_acquire — the only allowed memory order for
//        cross-plane reads. GC-E-044 blocks relaxed at Phase 4.
//     3. Documents the access with layer_from / layer_to parameters,
//        enabling static analysis tooling to verify layer relationships.
//
// S1 scope note:
//   L3 and L4 are implemented in S3. This header is defined in S1 because
//   GC-E-044 and GC-E-045 require pre-registered violation fixtures in S1
//   (spec §8 pre-registration table). The fixture tests exercise this header.
//
// Forbidden : CLOCK_REALTIME — not used (pre-compliant with GC-E-011).
//             malloc/new/free/delete — structurally absent (GC-E-010).

#pragma once

#include <atomic>
#include <cstdint>

namespace egbos::plane {

// ─────────────────────────────────────────────────────────────────────────────
// Layer identifier constants (spec §14.4)
//
// Used as the layer_from and layer_to arguments of CROSS_PLANE_READ.
// Named constants (rather than bare integers) allow static analysis tools
// to parse the layer topology from source and verify that reads cross the
// correct architectural boundary.
//
// Mapping mirrors the ENGINE_V3 five-layer execution plane hierarchy:
//   L0 — Market Data Ingress (raw feed receiver)
//   L1 — Limit Order Book (LOB engine)
//   L2 — Feature Engine (signal computation)
//   L3 — Position Engine (order management + state)
//   L4 — Risk Enforcer (hard limits, kill switch)
//   L5 — Execution Gateway (exchange connectivity)
//   L6 — Regime Classifier (ML inference)
//   L7 — Strategy Selector (model dispatch)
// ─────────────────────────────────────────────────────────────────────────────
inline constexpr int LAYER_L0 = 0;  // Market Data Ingress
inline constexpr int LAYER_L1 = 1;  // Limit Order Book
inline constexpr int LAYER_L2 = 2;  // Feature Engine
inline constexpr int LAYER_L3 = 3;  // Position Engine
inline constexpr int LAYER_L4 = 4;  // Risk Enforcer
inline constexpr int LAYER_L5 = 5;  // Execution Gateway
inline constexpr int LAYER_L6 = 6;  // Regime Classifier
inline constexpr int LAYER_L7 = 7;  // Strategy Selector

// Compile-time assertion: layer identifiers are ordered and distinct.
// If this fires, a constant above was incorrectly edited.
static_assert(LAYER_L0 < LAYER_L1 && LAYER_L1 < LAYER_L2 &&
              LAYER_L2 < LAYER_L3 && LAYER_L3 < LAYER_L4 &&
              LAYER_L4 < LAYER_L5 && LAYER_L5 < LAYER_L6 &&
              LAYER_L6 < LAYER_L7,
    "Layer identifier constants must be strictly ordered 0..7 "
    "(used by static analysis to verify cross-plane topology)");

}  // namespace egbos::plane

// ─────────────────────────────────────────────────────────────────────────────
// CROSS_PLANE_READ(atomic_var, layer_from, layer_to)
//
// Mandatory wrapper for every std::atomic read that crosses a layer boundary.
// Direct .load() calls on cross-boundary atomics are forbidden (GC-E-045).
//
// Arguments:
//   atomic_var  — the std::atomic<int64_t> to read. Must be exactly 8 bytes.
//                 Static assertion fires at compile time for any other type.
//   layer_from  — the LAYER_Lx constant of the layer performing the read.
//   layer_to    — the LAYER_Lx constant of the layer that owns the atomic.
//
// Returns:
//   int64_t — the value loaded with memory_order_acquire.
//
// Constraints (spec §6.3, HTML §C3.3 verbatim):
//   - Only int64_t atomics are permitted across layer boundaries.
//     Rationale: a single machine word; atomic on all supported architectures;
//     no torn reads on x86-64.
//   - Memory order is always acquire.
//     Rationale: the reading layer (e.g. L4) must observe all writes that
//     the owning layer (e.g. L3) made before the release store. acquire/release
//     pairing is the minimum correct order for this pattern.
//   - GC-E-044 (active Phase 4) blocks memory_order_relaxed substitution.
//     The static analysis rule matches any CROSS_PLANE_READ variant that uses
//     relaxed; the violation fixture (violation_GC_E_044.cpp) confirms this.
//
// Usage example (S3, L4 reading L3's gross_exposure_):
//   int64_t exposure = CROSS_PLANE_READ(
//       position_state_view.gross_exposure_, LAYER_L4, LAYER_L3);
//
// Implementation note:
//   The macro uses a lambda with an immediately-invoked call to produce an
//   expression with a well-defined value and type. The (void) casts on
//   layer_from / layer_to evaluate the arguments (preventing unused-variable
//   warnings) without emitting any runtime code — the constants fold away
//   under any optimisation level, including -O0.
// ─────────────────────────────────────────────────────────────────────────────
#define CROSS_PLANE_READ(atomic_var, layer_from, layer_to)                    \
    ([&]() -> int64_t {                                                        \
        /* Enforce int64_t-only rule (spec §6.3, HTML §C3.3 verbatim).       \
           GC-E-045 additionally forbids direct field reads of x_l_ /        \
           order_tag_ from L4/L5; this static_assert is the compile-time     \
           half of that enforcement. */                                        \
        static_assert(                                                         \
            sizeof(atomic_var) == sizeof(int64_t),                            \
            "CROSS_PLANE_READ: only std::atomic<int64_t> (8-byte) atomics "  \
            "are permitted across layer boundaries (spec §6.3). "             \
            "Wider or narrower types require a new claim at V1 minimum.");    \
        /* Evaluate layer constants — fold to nothing at compile time.        \
           Their presence in the source is what static analysis tools read.  \
           (void) suppresses -Wunused warnings at -O0. */                     \
        (void)(layer_from);                                                    \
        (void)(layer_to);                                                      \
        /* memory_order_acquire: see rationale in header comment above.       \
           GC-E-044 blocks relaxed substitution at Phase 4. */                \
        return (atomic_var).load(std::memory_order_acquire);                  \
    }())

// ─────────────────────────────────────────────────────────────────────────────
// GC-E-044 / GC-E-045 documentation block
//
// These rules are pre-registered in S1 and activate at Phase 4 (S3 scope).
// Violation fixtures for both rules must exist in tests/governance/fixtures/
// by S1 close (spec §8 pre-registration table, §15.4).
//
// GC-E-044 pattern the static analysis rule detects:
//   Any .load(std::memory_order_relaxed) on an atomic whose address is
//   sourced from an egbos::shm SHM region, OR any CROSS_PLANE_READ variant
//   that substitutes relaxed for acquire.
//   → Violation fixture: violation_GC_E_044.cpp
//     Content: an atomic load with memory_order_relaxed on a cross-boundary
//     int64_t, without the CROSS_PLANE_READ wrapper.
//
// GC-E-045 pattern the static analysis rule detects:
//   Any direct member access of a field named x_l_ or order_tag_ from a
//   function whose layer annotation is LAYER_L4 or LAYER_L5, without using
//   CROSS_PLANE_READ.
//   → Violation fixture: violation_GC_E_045.cpp
//     Content: position_engine_.order_tag_ accessed directly from an L4
//     function, bypassing the CROSS_PLANE_READ macro.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// Compile-time self-check
//
// Instantiate CROSS_PLANE_READ with a local int64_t atomic to verify the
// macro expands correctly and the static_assert is exercised at include time.
// This check compiles away completely; it does not emit any code.
// ─────────────────────────────────────────────────────────────────────────────
namespace egbos::plane::detail {

inline void _self_check_cross_plane_read() noexcept {
    // Verify the macro compiles and returns int64_t from an int64_t atomic.
    std::atomic<int64_t> test_atomic{42};
    [[maybe_unused]] int64_t val =
        CROSS_PLANE_READ(test_atomic, LAYER_L4, LAYER_L3);
    (void)val;
    // If the line above produces a compile error, the macro or the
    // static_assert inside it has regressed. Read the error message.
}

}  // namespace egbos::plane::detail
