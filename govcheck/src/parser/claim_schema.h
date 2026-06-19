// govcheck/src/parser/claim_schema.h
// Claim field enum + validation constants.

#pragma once

#include <string>
#include <vector>

namespace egbos::govcheck {

// Current supported schema version
constexpr int kSupportedSchemaVersion = 1;

// Maximum allowed title length
constexpr std::size_t kMaxTitleLength = 120;

// Required top-level fields in a claim YAML
inline std::vector<std::string> required_claim_fields() {
    return {
        "schema_version",
        "claim_id",
        "namespace",
        "title",
        "text",
        "verification_class",
        "class_rationale",
        "enforcement_mechanism",
        "enforcement_scope",
        "composed_verification_class",
        "owner",
        "review_authority",
        "remediation_category",
        "deployment_blocking",
        "minimum_class_for_unblocked_deploy",
        "created_at",
        "created_by",
        "last_reviewed",
        "review_expiry",
        "status",
    };
}

// Valid verification class strings
inline std::vector<std::string> valid_verification_classes() {
    return {"V1", "V2", "V3", "V4"};
}

// Valid status strings
inline std::vector<std::string> valid_statuses() {
    return {"active", "superseded", "archived", "invalidated"};
}

// Valid remediation category strings (null/empty is also valid)
inline std::vector<std::string> valid_remediation_categories() {
    return {"R3", "R4"};
}

}  // namespace egbos::govcheck
