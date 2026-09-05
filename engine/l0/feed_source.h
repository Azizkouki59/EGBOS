// engine/l0/feed_source.h
// EGBOS ENGINE_V3 — Feed Source Provenance Abstraction
//
// Governance   : GOV-S5-L0-ARCH-DECISIONS (EGBOS-GOV-DECISION-2026-009)
// Decisions    : D7 (REQUIRE_PROVENANCE_ABSTRACTION), D9 (REQUIRE_PROVENANCE_BINDING)
// Constraints  : Zero dynamic allocation (GC-E-010).
//
// Invariant    : TEST DATA != LIVE_OPERATIONAL EVIDENCE
//                REPLAY DATA != LIVE_OPERATIONAL EVIDENCE
//                LOCAL DATA != LIVE_OPERATIONAL EVIDENCE

#pragma once

#include <cstdint>

namespace egbos::l0 {

// Feed provenance classification.
// Structural enforcement: a DevelopmentTestFeed can never masquerade
// as a ProductionVerifiedFeed. Promotion to PRODUCTION_VERIFIED
// requires an explicit signed governance artifact (MARKET-DATA-001.yaml).
enum class FeedProvenance : uint8_t {
    DEVELOPMENT_TEST      = 0,   // Localhost, synthetic, unit test
    HISTORICAL_REPLAY     = 1,   // Tier-0 log replay
    EXTERNAL_LIVE         = 2,   // Connected to external exchange (not yet verified)
    PRODUCTION_VERIFIED   = 3    // Signed MARKET-DATA-001.yaml binding exists
};

// Listener operational state.
// D1: A failed multicast join MUST NOT be treated as operational.
enum class ListenerState : uint8_t {
    INITIALIZING                  = 0,
    OPERATIONAL                   = 1,
    NON_OPERATIONAL_MULTICAST_FAILED = 2,
    FAULTED                       = 3
};

} // namespace egbos::l0
