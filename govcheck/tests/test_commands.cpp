// govcheck/tests/test_commands.cpp
// GoogleTest: CLI command entry points
// Spec: S0 Implementation Specification §7.1, §4.6

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "commands/cmd_claims.h"
#include "commands/cmd_status.h"
#include "commands/cmd_validate.h"

namespace egbos::govcheck {
namespace {

// Build a minimal repo tree under temp_root with one valid claim file.
void make_minimal_repo(const std::filesystem::path& root) {
    std::filesystem::create_directories(root / "governance" / "claims");
    std::filesystem::create_directories(root / "governance" / "proof_artifacts");

    // Minimal valid claim — claim_id must match filename stem.
    const std::string id = "TEST-CMD-001";
    std::ofstream claim(root / "governance" / "claims" / (id + ".yaml"));
    claim <<
        "schema_version: 1\n"
        "claim_id: \"" + id + "\"\n"
        "namespace: \"bootstrap\"\n"
        "title: \"Test command claim\"\n"
        "text: \"Exists for command-level testing.\"\n"
        "verification_class: \"V1\"\n"
        "class_rationale: \"Manual review.\"\n"
        "enforcement_mechanism: \"govcheck validate --scope C0\"\n"
        "enforcement_scope: \"All claims.\"\n"
        "composed_verification_class: \"V1\"\n"
        "owner: \"founder\"\n"
        "review_authority: \"founder\"\n"
        "remediation_category: null\n"
        "remediation_notes: \"\"\n"
        "deployment_blocking: false\n"
        "deployment_blocking_rationale: \"\"\n"
        "minimum_class_for_unblocked_deploy: \"V1\"\n"
        "rfc_references: []\n"
        "proof_artifacts: []\n"
        "review_history: []\n"
        "created_at: \"2026-06-17T00:00:00Z\"\n"
        "created_by: \"founder\"\n"
        "last_reviewed: \"2026-06-17T00:00:00Z\"\n"
        "review_expiry: \"2030-01-01T00:00:00Z\"\n"
        "status: \"active\"\n"
        "superseded_by: null\n"
        "amendments: []\n";

    // Minimal team.yaml
    std::ofstream team(root / "governance" / "team.yaml");
    team << "members:\n  - username: founder\n    role: Founder\n    active: true\n";

    // Minimal namespace-deps.yaml
    std::ofstream ns(root / "governance" / "namespace-deps.yaml");
    ns << "namespaces:\n  bootstrap:\n    allowed_dependencies: []\n";
}

}  // namespace

// ── cmd_status ────────────────────────────────────────────────────────────────

// cmd_status with a valid repo returns 0 (informational, always succeeds).
TEST(CommandsTest, CmdStatusValidRepo) {
    auto root = std::filesystem::temp_directory_path() / "test_cmd_status";
    make_minimal_repo(root);
    int rc = cmd_status(root);
    EXPECT_EQ(rc, 0);
    std::filesystem::remove_all(root);
}

// cmd_status against a directory with no claims still returns 0.
TEST(CommandsTest, CmdStatusEmptyRepo) {
    auto root = std::filesystem::temp_directory_path() / "test_cmd_status_empty";
    std::filesystem::create_directories(root / "governance" / "claims");
    int rc = cmd_status(root);
    EXPECT_EQ(rc, 0);
    std::filesystem::remove_all(root);
}

// ── cmd_claims ────────────────────────────────────────────────────────────────

// cmd_claims with no filters returns 0.
TEST(CommandsTest, CmdClaimsNoFilter) {
    auto root = std::filesystem::temp_directory_path() / "test_cmd_claims";
    make_minimal_repo(root);
    int rc = cmd_claims("", "", root);
    EXPECT_EQ(rc, 0);
    std::filesystem::remove_all(root);
}

// cmd_claims with a matching namespace filter returns 0.
TEST(CommandsTest, CmdClaimsNamespaceFilter) {
    auto root = std::filesystem::temp_directory_path() / "test_cmd_claims_ns";
    make_minimal_repo(root);
    int rc = cmd_claims("bootstrap", "", root);
    EXPECT_EQ(rc, 0);
    std::filesystem::remove_all(root);
}

// cmd_claims with non-matching namespace filter returns 0 (empty result).
TEST(CommandsTest, CmdClaimsNamespaceFilterNoMatch) {
    auto root = std::filesystem::temp_directory_path() / "test_cmd_claims_ns_nomatch";
    make_minimal_repo(root);
    int rc = cmd_claims("nonexistent_namespace", "", root);
    EXPECT_EQ(rc, 0);
    std::filesystem::remove_all(root);
}

}  // namespace egbos::govcheck
