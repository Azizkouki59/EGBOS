// govcheck/src/validator/claim_validator.cpp

#include "claim_validator.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace egbos::govcheck {

std::string to_string(Severity s) {
    switch (s) {
        case Severity::Fatal:   return "fatal";
        case Severity::Error:   return "error";
        case Severity::Warning: return "warning";
        default:                return "unknown";
    }
}

// ── Internal helpers ──────────────────────────────────────────────────────────

static int vc_rank(VerificationClass vc) {
    switch (vc) {
        case VerificationClass::V1: return 1;
        case VerificationClass::V2: return 2;
        case VerificationClass::V3: return 3;
        case VerificationClass::V4: return 4;
        default:                    return 0;
    }
}

// Parse ISO-8601 date string (YYYY-MM-DDThh:mm:ssZ) to time_t
static bool parse_iso8601(const std::string& s, std::time_t& out) {
    std::tm tm{};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
        // Try date-only
        std::istringstream ss2(s);
        ss2 >> std::get_time(&tm, "%Y-%m-%d");
        if (ss2.fail()) return false;
    }
    tm.tm_isdst = -1;
    out = std::mktime(&tm);
    return out != -1;
}

// ── validate_expiry ───────────────────────────────────────────────────────────

std::vector<ValidationFinding> validate_expiry(const ParsedClaim& claim) {
    std::vector<ValidationFinding> findings;

    std::time_t expiry_t;
    if (!parse_iso8601(claim.review_expiry, expiry_t)) {
        findings.push_back(
            {"GC-E-020", Severity::Warning, claim.claim_id,
             "Could not parse review_expiry: '" + claim.review_expiry + "'",
             claim.source_file.string(), 0});
        return findings;
    }

    std::time_t now = std::time(nullptr);
    if (now > expiry_t) {
        findings.push_back(
            {"GC-E-020", Severity::Warning, claim.claim_id,
             "review_expiry '" + claim.review_expiry + "' has been exceeded",
             claim.source_file.string(), 0});
    }

    return findings;
}

// ── validate_composed_class ───────────────────────────────────────────────────

std::vector<ValidationFinding> validate_composed_class(
    const ParsedClaim& claim, const std::vector<ParsedClaim>& all_claims) {
    std::vector<ValidationFinding> findings;

    // Compute minimum class across all transitive dependencies
    // For S0 with no dependencies, composed == own class
    int own_rank = vc_rank(claim.verification_class);
    int min_rank = own_rank;

    for (const auto& dep : claim.composition_dependencies) {
        // Find dependency in all_claims
        auto it = std::find_if(all_claims.begin(), all_claims.end(),
                               [&](const ParsedClaim& c) { return c.claim_id == dep.claim_id; });
        if (it != all_claims.end()) {
            int dep_rank = vc_rank(it->verification_class);
            if (dep_rank < min_rank) min_rank = dep_rank;
        }
    }

    int declared_composed_rank = vc_rank(claim.composed_verification_class);

    // GC-E-024: composed class exceeds declared class
    if (declared_composed_rank > own_rank) {
        findings.push_back(
            {"GC-E-024", Severity::Error, claim.claim_id,
             "composed_verification_class (" +
                 to_string(claim.composed_verification_class) +
                 ") exceeds verification_class (" +
                 to_string(claim.verification_class) + ")",
             claim.source_file.string(), 0});
    }

    // GC-E-017: declared composed class disagrees with computed minimum
    if (declared_composed_rank != min_rank) {
        // Convert min_rank back to enum for message
        VerificationClass computed_vc = VerificationClass::Unknown;
        switch (min_rank) {
            case 1: computed_vc = VerificationClass::V1; break;
            case 2: computed_vc = VerificationClass::V2; break;
            case 3: computed_vc = VerificationClass::V3; break;
            case 4: computed_vc = VerificationClass::V4; break;
            default: break;
        }
        findings.push_back(
            {"GC-E-017", Severity::Error, claim.claim_id,
             "composed_verification_class (" +
                 to_string(claim.composed_verification_class) +
                 ") does not match computed minimum (" +
                 to_string(computed_vc) + ")",
             claim.source_file.string(), 0});
    }

    return findings;
}

// ── validate_claim ────────────────────────────────────────────────────────────

std::vector<ValidationFinding> validate_claim(
    const ParsedClaim& claim,
    const std::vector<std::string>& team_members,
    const std::vector<ParsedClaim>& all_claims) {

    std::vector<ValidationFinding> findings;

    // GC-E-019: owner must be in team.yaml
    // Solo founder mode: "founder" always passes
    if (claim.owner != "founder") {
        auto it = std::find(team_members.begin(), team_members.end(), claim.owner);
        if (it == team_members.end()) {
            findings.push_back(
                {"GC-E-019", Severity::Error, claim.claim_id,
                 "Claim owner '" + claim.owner + "' not found in team.yaml",
                 claim.source_file.string(), 0});
        }
    }

    // GC-E-025: no active claim may depend on a superseded claim
    for (const auto& dep : claim.composition_dependencies) {
        auto it = std::find_if(all_claims.begin(), all_claims.end(),
                               [&](const ParsedClaim& c) { return c.claim_id == dep.claim_id; });
        if (it != all_claims.end() && it->status == ClaimStatus::Superseded) {
            findings.push_back(
                {"GC-E-025", Severity::Error, claim.claim_id,
                 "Claim depends on superseded claim '" + dep.claim_id + "'",
                 claim.source_file.string(), 0});
        }
    }

    // Composed class validation (GC-E-017, GC-E-024)
    auto composed_findings = validate_composed_class(claim, all_claims);
    findings.insert(findings.end(), composed_findings.begin(), composed_findings.end());

    // Expiry check (GC-E-020) — Warning at S0, blocking from S1
    auto expiry_findings = validate_expiry(claim);
    findings.insert(findings.end(), expiry_findings.begin(), expiry_findings.end());

    return findings;
}

}  // namespace egbos::govcheck
