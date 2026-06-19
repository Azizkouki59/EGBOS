// govcheck/src/validator/claim_validator.h
// Field presence, type, enum checks, composed class verification.

#pragma once

#include <string>
#include <vector>

#include "parser/yaml_parser.h"

namespace egbos::govcheck {

// ── ValidationFinding ─────────────────────────────────────────────────────────

enum class Severity { Fatal, Error, Warning };

struct ValidationFinding {
    std::string code;
    Severity severity{Severity::Error};
    std::string claim_id;
    std::string message;
    std::string file;
    int line{0};
};

// ── ClaimValidator ────────────────────────────────────────────────────────────

// Validate a single parsed claim against governance rules.
// Requires: team members list (for GC-E-019), all parsed claims (for GC-E-025).
std::vector<ValidationFinding> validate_claim(
    const ParsedClaim& claim,
    const std::vector<std::string>& team_members,
    const std::vector<ParsedClaim>& all_claims);

// Validate composed_verification_class against computed minimum.
// GC-E-017: human-declared composed class disagrees with computed value.
// GC-E-024: composed class exceeds declared class.
std::vector<ValidationFinding> validate_composed_class(
    const ParsedClaim& claim,
    const std::vector<ParsedClaim>& all_claims);

// Check review_expiry has not been exceeded.
// GC-E-020: review_expiry exceeded.
std::vector<ValidationFinding> validate_expiry(const ParsedClaim& claim);

// Helper: convert Severity to string
std::string to_string(Severity s);

}  // namespace egbos::govcheck
