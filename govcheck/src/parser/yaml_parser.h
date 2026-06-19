// govcheck/src/parser/yaml_parser.h
// YAML loading and schema validation for governance claims.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace egbos::govcheck {

// ── Enums ────────────────────────────────────────────────────────────────────

enum class VerificationClass { V1, V2, V3, V4, Unknown };
enum class ClaimStatus { Active, Superseded, Archived, Invalidated, Unknown };
enum class RemediationCategory { None, R3, R4, Unknown };

// ── Sub-structs ───────────────────────────────────────────────────────────────

struct ReviewHistoryEntry {
    std::string date;
    std::string reviewer;
    std::string action;
    std::string class_at_review;
    std::string notes;
};

struct ProofArtifact {
    std::string path;
    std::string type;
    std::string sha256;
};

struct CompositionDependency {
    std::string claim_id;
};

// ── ParsedClaim ───────────────────────────────────────────────────────────────

struct ParsedClaim {
    int schema_version{0};
    std::string claim_id;
    std::string namespace_id;
    std::string title;
    std::string text;

    VerificationClass verification_class{VerificationClass::Unknown};
    std::string previous_class;
    std::string class_rationale;
    std::string enforcement_mechanism;
    std::string enforcement_scope;

    std::vector<std::string> enforcement_gaps;
    std::vector<std::string> assumptions;
    std::vector<CompositionDependency> composition_dependencies;

    VerificationClass composed_verification_class{VerificationClass::Unknown};
    std::string owner;
    std::string review_authority;
    std::string backup_owner;

    RemediationCategory remediation_category{RemediationCategory::None};
    std::string remediation_notes;

    bool deployment_blocking{false};
    std::string deployment_blocking_rationale;
    std::string minimum_class_for_unblocked_deploy;

    std::vector<std::string> rfc_references;
    std::vector<ProofArtifact> proof_artifacts;
    std::vector<ReviewHistoryEntry> review_history;

    std::string created_at;
    std::string created_by;
    std::string last_reviewed;
    std::string review_expiry;

    ClaimStatus status{ClaimStatus::Unknown};
    std::string superseded_by;
    std::vector<std::string> amendments;

    // Source file path (set by parser)
    std::filesystem::path source_file;
};

// ── ParseError ────────────────────────────────────────────────────────────────

struct ParseError {
    std::string file;
    int line{0};
    std::string field;
    std::string message;
};

// ── ParseResult ───────────────────────────────────────────────────────────────

struct ParseResult {
    std::optional<ParsedClaim> claim;
    std::vector<ParseError> errors;

    bool ok() const { return claim.has_value() && errors.empty(); }
};

// ── Parser API ────────────────────────────────────────────────────────────────

// Parse a single claim YAML file.
// Validates schema version, required fields, enum values.
// Returns ParseResult with claim on success or errors on failure.
ParseResult parse_claim_file(const std::filesystem::path& path);

// Load all claims from a directory (non-recursive, *.yaml only).
// Returns vector of ParseResult — one per file found.
std::vector<ParseResult> load_claims_from_dir(const std::filesystem::path& dir);

// Convert string to VerificationClass enum. Returns Unknown on invalid input.
VerificationClass parse_verification_class(const std::string& s);

// Convert string to ClaimStatus enum. Returns Unknown on invalid input.
ClaimStatus parse_claim_status(const std::string& s);

// Convert string to RemediationCategory. Returns Unknown on invalid input.
RemediationCategory parse_remediation_category(const std::string& s);

// Convert VerificationClass to string.
std::string to_string(VerificationClass vc);

// Convert ClaimStatus to string.
std::string to_string(ClaimStatus cs);

// Convert RemediationCategory to string.
std::string to_string(RemediationCategory rc);

}  // namespace egbos::govcheck
