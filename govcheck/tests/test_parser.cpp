// govcheck/tests/test_parser.cpp
// GoogleTest: YAML parsing
// Spec: S0 Implementation Specification §7.1

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "parser/yaml_parser.h"

namespace egbos::govcheck {
namespace {

// Write YAML content to a temp file named <stem>.yaml.
// The stem must equal the claim_id field to satisfy the filename check.
std::filesystem::path write_temp(const std::string& stem, const std::string& content) {
    auto path = std::filesystem::temp_directory_path() / (stem + ".yaml");
    std::ofstream f(path, std::ios::trunc);
    f << content;
    return path;
}

// Build a minimal, fully-valid claim YAML for the given claim_id.
std::string valid_yaml(const std::string& claim_id,
                       const std::string& vc = "V1",
                       const std::string& title = "Test claim title") {
    return
        "schema_version: 1\n"
        "claim_id: \"" + claim_id + "\"\n"
        "namespace: \"bootstrap\"\n"
        "title: \"" + title + "\"\n"
        "text: \"Test claim body.\"\n"
        "verification_class: \"" + vc + "\"\n"
        "class_rationale: \"Manual review.\"\n"
        "enforcement_mechanism: \"govcheck validate --scope C0\"\n"
        "enforcement_scope: \"All claims.\"\n"
        "composed_verification_class: \"" + vc + "\"\n"
        "owner: \"founder\"\n"
        "review_authority: \"founder\"\n"
        "remediation_category: null\n"
        "remediation_notes: \"\"\n"
        "deployment_blocking: false\n"
        "deployment_blocking_rationale: \"\"\n"
        "minimum_class_for_unblocked_deploy: \"" + vc + "\"\n"
        "rfc_references: []\n"
        "proof_artifacts: []\n"
        "review_history: []\n"
        "created_at: \"2026-06-17T00:00:00Z\"\n"
        "created_by: \"founder\"\n"
        "last_reviewed: \"2026-06-17T00:00:00Z\"\n"
        "review_expiry: \"2027-06-17T00:00:00Z\"\n"
        "status: \"active\"\n"
        "superseded_by: null\n"
        "amendments: []\n";
}

}  // namespace

// Parse valid claim → returns ParsedClaim, no errors
TEST(ParserTest, ParseValidClaim) {
    const std::string id = "PARSE-VALID-001";
    auto path = write_temp(id, valid_yaml(id));
    auto result = parse_claim_file(path);
    ASSERT_TRUE(result.errors.empty())
        << "Unexpected error: " << result.errors[0].message;
    ASSERT_TRUE(result.claim.has_value());
    EXPECT_EQ(result.claim->claim_id, id);
    EXPECT_EQ(result.claim->verification_class, VerificationClass::V1);
    EXPECT_EQ(result.claim->status, ClaimStatus::Active);
}

// Parse claim with missing required field → returns specific field error
TEST(ParserTest, RejectMissingFields) {
    const std::string id = "MISSING-FIELDS-001";
    // Provide only schema_version, claim_id, namespace — title and all below missing.
    const std::string yaml =
        "schema_version: 1\n"
        "claim_id: \"" + id + "\"\n"
        "namespace: \"bootstrap\"\n";
    auto path = write_temp(id, yaml);
    auto result = parse_claim_file(path);
    EXPECT_FALSE(result.ok());
    ASSERT_FALSE(result.errors.empty());
    // First missing field after namespace is title.
    EXPECT_NE(result.errors[0].message.find("title"), std::string::npos);
}

// Parse claim with schema_version: 99 → rejects unknown version
TEST(ParserTest, RejectUnknownSchemaVersion) {
    const std::string id = "UNKNOWN-VER-001";
    const std::string yaml =
        "schema_version: 99\n"
        "claim_id: \"" + id + "\"\n"
        "namespace: \"bootstrap\"\n"
        "title: \"Test\"\n";
    auto path = write_temp(id, yaml);
    auto result = parse_claim_file(path);
    EXPECT_FALSE(result.ok());
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].message.find("Unknown schema version"), std::string::npos);
}

// Parse claim with verification_class: V9 → returns enum validation error
TEST(ParserTest, RejectInvalidEnum) {
    const std::string id = "INVALID-ENUM-001";
    // Build valid yaml then substitute the standalone verification_class value.
    std::string yaml = valid_yaml(id);
    // Replace "verification_class: "V1"" (first occurrence, not composed_).
    const std::string target = "verification_class: \"V1\"";
    const std::string replacement = "verification_class: \"V9\"";
    auto pos = yaml.find(target);
    ASSERT_NE(pos, std::string::npos);
    yaml.replace(pos, target.size(), replacement);
    auto path = write_temp(id, yaml);
    auto result = parse_claim_file(path);
    EXPECT_FALSE(result.ok());
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].message.find("Invalid verification_class"), std::string::npos);
}

// Parse claim where claim_id ≠ filename → returns mismatch error
TEST(ParserTest, RejectClaimIdMismatch) {
    // File stem is WRONG-001 but inner claim_id is RIGHT-001.
    const std::string file_stem = "WRONG-001";
    const std::string inner_id  = "RIGHT-001";
    auto path = write_temp(file_stem, valid_yaml(inner_id));
    auto result = parse_claim_file(path);
    EXPECT_FALSE(result.ok());
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].message.find("does not match filename"), std::string::npos);
}

// Parse claim with title > 120 chars → returns length error
TEST(ParserTest, RejectLongTitle) {
    const std::string id = "LONG-TITLE-001";
    const std::string long_title(150, 'A');
    auto path = write_temp(id, valid_yaml(id, "V1", long_title));
    auto result = parse_claim_file(path);
    EXPECT_FALSE(result.ok());
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].message.find("title exceeds 120 characters"), std::string::npos);
}

}  // namespace egbos::govcheck
