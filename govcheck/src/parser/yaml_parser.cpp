// govcheck/src/parser/yaml_parser.cpp

#include "yaml_parser.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace egbos::govcheck {

// ── Enum conversions ──────────────────────────────────────────────────────────

VerificationClass parse_verification_class(const std::string& s) {
    if (s == "V1") return VerificationClass::V1;
    if (s == "V2") return VerificationClass::V2;
    if (s == "V3") return VerificationClass::V3;
    if (s == "V4") return VerificationClass::V4;
    return VerificationClass::Unknown;
}

ClaimStatus parse_claim_status(const std::string& s) {
    if (s == "active")      return ClaimStatus::Active;
    if (s == "superseded")  return ClaimStatus::Superseded;
    if (s == "archived")    return ClaimStatus::Archived;
    if (s == "invalidated") return ClaimStatus::Invalidated;
    return ClaimStatus::Unknown;
}

RemediationCategory parse_remediation_category(const std::string& s) {
    if (s.empty() || s == "null" || s == "~") return RemediationCategory::None;
    if (s == "R3") return RemediationCategory::R3;
    if (s == "R4") return RemediationCategory::R4;
    return RemediationCategory::Unknown;
}

std::string to_string(VerificationClass vc) {
    switch (vc) {
        case VerificationClass::V1: return "V1";
        case VerificationClass::V2: return "V2";
        case VerificationClass::V3: return "V3";
        case VerificationClass::V4: return "V4";
        default:                    return "Unknown";
    }
}

std::string to_string(ClaimStatus cs) {
    switch (cs) {
        case ClaimStatus::Active:      return "active";
        case ClaimStatus::Superseded:  return "superseded";
        case ClaimStatus::Archived:    return "archived";
        case ClaimStatus::Invalidated: return "invalidated";
        default:                       return "unknown";
    }
}

std::string to_string(RemediationCategory rc) {
    switch (rc) {
        case RemediationCategory::None: return "";
        case RemediationCategory::R3:   return "R3";
        case RemediationCategory::R4:   return "R4";
        default:                        return "unknown";
    }
}

// ── Internal helpers ──────────────────────────────────────────────────────────

static std::string node_as_string(const YAML::Node& node) {
    if (!node || node.IsNull()) return "";
    return node.as<std::string>("");
}

static bool has_required(const YAML::Node& root, const std::string& key,
                          const std::string& file, std::vector<ParseError>& errors) {
    if (!root[key] || root[key].IsNull()) {
        errors.push_back({file, 0, key, "Required field '" + key + "' is missing or null"});
        return false;
    }
    return true;
}

static ParseResult parse_claim_node(const YAML::Node& root, const std::filesystem::path& path, bool check_filename) {
    ParseResult result;
    const std::string file_str = path.string();

    ParsedClaim claim;
    claim.source_file = path;

    // ── schema_version ──
    if (!has_required(root, "schema_version", file_str, result.errors)) return result;
    int sv = root["schema_version"].as<int>(0);
    if (sv != 1) {
        result.errors.push_back(
            {file_str, 0, "schema_version",
             "Unknown schema version: " + std::to_string(sv) + " (expected 1)"});
        return result;
    }
    claim.schema_version = sv;

    // ── claim_id ──
    if (!has_required(root, "claim_id", file_str, result.errors)) return result;
    claim.claim_id = node_as_string(root["claim_id"]);

    // Validate claim_id matches filename (without .yaml)
    if (check_filename) {
        std::string expected_id = path.stem().string();
        if (claim.claim_id != expected_id) {
            result.errors.push_back(
                {file_str, 0, "claim_id",
                 "claim_id '" + claim.claim_id + "' does not match filename '" + expected_id + "'"});
            return result;
        }
    }

    // ── namespace ──
    if (!has_required(root, "namespace", file_str, result.errors)) return result;
    claim.namespace_id = node_as_string(root["namespace"]);

    // ── title ──
    if (!has_required(root, "title", file_str, result.errors)) return result;
    claim.title = node_as_string(root["title"]);
    if (claim.title.size() > 120) {
        result.errors.push_back(
            {file_str, 0, "title",
             "title exceeds 120 characters (length: " + std::to_string(claim.title.size()) + ")"});
        return result;
    }

    // ── text ──
    if (!has_required(root, "text", file_str, result.errors)) return result;
    claim.text = node_as_string(root["text"]);

    // ── verification_class ──
    if (!has_required(root, "verification_class", file_str, result.errors)) return result;
    std::string vc_str = node_as_string(root["verification_class"]);
    claim.verification_class = parse_verification_class(vc_str);
    if (claim.verification_class == VerificationClass::Unknown) {
        result.errors.push_back(
            {file_str, 0, "verification_class",
             "Invalid verification_class: '" + vc_str + "' (must be V1, V2, V3, or V4)"});
        return result;
    }

    // ── previous_class (optional) ──
    if (root["previous_class"] && !root["previous_class"].IsNull()) {
        claim.previous_class = node_as_string(root["previous_class"]);
    }

    // ── class_rationale ──
    if (!has_required(root, "class_rationale", file_str, result.errors)) return result;
    claim.class_rationale = node_as_string(root["class_rationale"]);

    // ── enforcement_mechanism ──
    if (!has_required(root, "enforcement_mechanism", file_str, result.errors)) return result;
    claim.enforcement_mechanism = node_as_string(root["enforcement_mechanism"]);

    // ── enforcement_scope ──
    if (!has_required(root, "enforcement_scope", file_str, result.errors)) return result;
    claim.enforcement_scope = node_as_string(root["enforcement_scope"]);

    // ── enforcement_gaps (list) ──
    if (root["enforcement_gaps"] && root["enforcement_gaps"].IsSequence()) {
        for (const auto& item : root["enforcement_gaps"]) {
            claim.enforcement_gaps.push_back(node_as_string(item));
        }
    }

    // ── assumptions (list) ──
    if (root["assumptions"] && root["assumptions"].IsSequence()) {
        for (const auto& item : root["assumptions"]) {
            claim.assumptions.push_back(node_as_string(item));
        }
    }

    // ── composition_dependencies (list) ──
    if (root["composition_dependencies"] && root["composition_dependencies"].IsSequence()) {
        for (const auto& item : root["composition_dependencies"]) {
            CompositionDependency dep;
            if (item.IsMap() && item["claim_id"]) {
                dep.claim_id = node_as_string(item["claim_id"]);
            } else if (item.IsScalar()) {
                dep.claim_id = node_as_string(item);
            }
            if (!dep.claim_id.empty()) {
                claim.composition_dependencies.push_back(dep);
            }
        }
    }

    // ── composed_verification_class ──
    if (!has_required(root, "composed_verification_class", file_str, result.errors)) return result;
    std::string cvc_str = node_as_string(root["composed_verification_class"]);
    claim.composed_verification_class = parse_verification_class(cvc_str);
    if (claim.composed_verification_class == VerificationClass::Unknown) {
        result.errors.push_back(
            {file_str, 0, "composed_verification_class",
             "Invalid composed_verification_class: '" + cvc_str + "'"});
        return result;
    }

    // ── owner ──
    if (!has_required(root, "owner", file_str, result.errors)) return result;
    claim.owner = node_as_string(root["owner"]);

    // ── review_authority ──
    if (!has_required(root, "review_authority", file_str, result.errors)) return result;
    claim.review_authority = node_as_string(root["review_authority"]);

    // ── backup_owner (optional) ──
    if (root["backup_owner"] && !root["backup_owner"].IsNull()) {
        claim.backup_owner = node_as_string(root["backup_owner"]);
    }

    // ── remediation_category ──
    if (!root["remediation_category"].IsDefined()) {
        result.errors.push_back(
            {file_str, 0, "remediation_category",
             "Required field 'remediation_category' is missing"});
        return result;
    }
    std::string rc_str;
    if (!root["remediation_category"].IsNull()) {
        rc_str = node_as_string(root["remediation_category"]);
    }
    claim.remediation_category = parse_remediation_category(rc_str);
    if (claim.remediation_category == RemediationCategory::Unknown) {
        result.errors.push_back(
            {file_str, 0, "remediation_category",
             "Invalid remediation_category: '" + rc_str + "' (must be null, R3, or R4)"});
        return result;
    }

    // ── remediation_notes ──
    if (root["remediation_notes"]) {
        claim.remediation_notes = node_as_string(root["remediation_notes"]);
    }
    // Mandatory when remediation_category non-null
    if (claim.remediation_category != RemediationCategory::None &&
        claim.remediation_notes.empty()) {
        result.errors.push_back(
            {file_str, 0, "remediation_notes",
             "remediation_notes is mandatory when remediation_category is specified"});
        return result;
    }

    // ── deployment_blocking ──
    if (root["deployment_blocking"]) {
        claim.deployment_blocking = root["deployment_blocking"].as<bool>(false);
    }

    // ── deployment_blocking_rationale ──
    if (root["deployment_blocking_rationale"]) {
        claim.deployment_blocking_rationale =
            node_as_string(root["deployment_blocking_rationale"]);
    }
    // Mandatory when deployment_blocking is true
    if (claim.deployment_blocking && claim.deployment_blocking_rationale.empty()) {
        result.errors.push_back(
            {file_str, 0, "deployment_blocking_rationale",
             "deployment_blocking_rationale is mandatory when deployment_blocking is true"});
        return result;
    }

    // ── minimum_class_for_unblocked_deploy ──
    if (!has_required(root, "minimum_class_for_unblocked_deploy", file_str, result.errors))
        return result;
    claim.minimum_class_for_unblocked_deploy =
        node_as_string(root["minimum_class_for_unblocked_deploy"]);

    // ── rfc_references (list, optional) ──
    if (root["rfc_references"] && root["rfc_references"].IsSequence()) {
        for (const auto& item : root["rfc_references"]) {
            claim.rfc_references.push_back(node_as_string(item));
        }
    }

    // ── proof_artifacts (list) ──
    if (root["proof_artifacts"] && root["proof_artifacts"].IsSequence()) {
        for (const auto& item : root["proof_artifacts"]) {
            ProofArtifact pa;
            if (!item["path"] || !item["type"] || !item["sha256"]) {
                result.errors.push_back(
                    {file_str, 0, "proof_artifacts",
                     "Each proof_artifact must have 'path', 'type', and 'sha256' fields"});
                return result;
            }
            pa.path   = node_as_string(item["path"]);
            pa.type   = node_as_string(item["type"]);
            pa.sha256 = node_as_string(item["sha256"]);
            claim.proof_artifacts.push_back(pa);
        }
    }

    // ── review_history (list) ──
    if (root["review_history"] && root["review_history"].IsSequence()) {
        for (const auto& item : root["review_history"]) {
            ReviewHistoryEntry rhe;
            rhe.date           = node_as_string(item["date"]);
            rhe.reviewer       = node_as_string(item["reviewer"]);
            rhe.action         = node_as_string(item["action"]);
            rhe.class_at_review = node_as_string(item["class_at_review"]);
            rhe.notes          = node_as_string(item["notes"]);
            claim.review_history.push_back(rhe);
        }
    }

    // ── timestamps ──
    if (!has_required(root, "created_at", file_str, result.errors)) return result;
    claim.created_at = node_as_string(root["created_at"]);

    if (!has_required(root, "created_by", file_str, result.errors)) return result;
    claim.created_by = node_as_string(root["created_by"]);

    if (!has_required(root, "last_reviewed", file_str, result.errors)) return result;
    claim.last_reviewed = node_as_string(root["last_reviewed"]);

    if (!has_required(root, "review_expiry", file_str, result.errors)) return result;
    claim.review_expiry = node_as_string(root["review_expiry"]);

    // ── status ──
    if (!has_required(root, "status", file_str, result.errors)) return result;
    std::string status_str = node_as_string(root["status"]);
    claim.status = parse_claim_status(status_str);
    if (claim.status == ClaimStatus::Unknown) {
        result.errors.push_back(
            {file_str, 0, "status",
             "Invalid status: '" + status_str +
                 "' (must be active, superseded, archived, or invalidated)"});
        return result;
    }

    // ── superseded_by (optional) ──
    if (root["superseded_by"] && !root["superseded_by"].IsNull()) {
        claim.superseded_by = node_as_string(root["superseded_by"]);
    }

    // ── amendments (list, optional) ──
    if (root["amendments"] && root["amendments"].IsSequence()) {
        for (const auto& item : root["amendments"]) {
            claim.amendments.push_back(node_as_string(item));
        }
    }

    result.claim = std::move(claim);
    return result;
}

// ── parse_claim_file ──────────────────────────────────────────────────────────

ParseResult parse_claim_file(const std::filesystem::path& path) {
    ParseResult result;
    try {
        YAML::Node root = YAML::LoadFile(path.string());
        return parse_claim_node(root, path, true);
    } catch (const YAML::Exception& e) {
        result.errors.push_back({path.string(), e.mark.line + 1, "", e.msg});
        return result;
    }
}

// ── load_claims_from_dir ──────────────────────────────────────────────────────

std::vector<ParseResult> load_claims_from_dir(const std::filesystem::path& dir) {
    std::vector<ParseResult> results;

    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        ParseResult err;
        err.errors.push_back(
            {dir.string(), 0, "", "Directory does not exist: " + dir.string()});
        results.push_back(std::move(err));
        return results;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".yaml") {
            try {
                std::vector<YAML::Node> docs = YAML::LoadAllFromFile(entry.path().string());
                for (std::size_t i = 0; i < docs.size(); ++i) {
                    results.push_back(parse_claim_node(docs[i], entry.path(), i == 0));
                }
            } catch (const YAML::Exception& e) {
                ParseResult err;
                err.errors.push_back({entry.path().string(), e.mark.line + 1, "", e.msg});
                results.push_back(std::move(err));
            }
        }
    }

    return results;
}

}  // namespace egbos::govcheck
