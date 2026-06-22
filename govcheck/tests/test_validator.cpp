// govcheck/tests/test_validator.cpp
// GoogleTest: Claim and Artifact Validation
// Spec: S0 Implementation Specification §7.1

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "parser/yaml_parser.h"
#include "validator/claim_validator.h"
#include "validator/artifact_validator.h"

namespace egbos::govcheck {
namespace {

// Create a mock claim helper
ParsedClaim make_mock_claim(const std::string& id, VerificationClass vc) {
    ParsedClaim c;
    c.claim_id = id;
    c.verification_class = vc;
    c.composed_verification_class = vc;
    c.owner = "founder";
    c.review_expiry = "2030-01-01T00:00:00Z";
    c.status = ClaimStatus::Active;
    c.source_file = "claims/" + id + ".yaml";
    return c;
}

}  // namespace

// Test validate_expiry logic (GC-E-020)
TEST(ValidatorTest, ValidateExpiry) {
    auto claim = make_mock_claim("EXPIRY-001", VerificationClass::V1);
    
    // Future expiry should pass
    auto findings = validate_expiry(claim);
    EXPECT_TRUE(findings.empty());

    // Exceeded expiry should produce GC-E-020 warning
    claim.review_expiry = "2020-01-01T00:00:00Z";
    findings = validate_expiry(claim);
    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].code, "GC-E-020");
    EXPECT_EQ(findings[0].severity, Severity::Warning);

    // Unparseable expiry should produce GC-E-020 warning
    claim.review_expiry = "invalid-date-format";
    findings = validate_expiry(claim);
    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].code, "GC-E-020");
}

// Test validate_composed_class logic (GC-E-024, GC-E-017)
TEST(ValidatorTest, ComposedClassValidation) {
    // Case 1: Standard valid claim with no dependencies
    auto claim = make_mock_claim("COMP-001", VerificationClass::V2);
    auto findings = validate_composed_class(claim, {claim});
    EXPECT_TRUE(findings.empty());

    // Case 2: Composed class exceeds own class (GC-E-024)
    claim.verification_class = VerificationClass::V1;
    claim.composed_verification_class = VerificationClass::V2;
    findings = validate_composed_class(claim, {claim});
    ASSERT_FALSE(findings.empty());
    bool found_gce024 = false;
    for (const auto& f : findings) {
        if (f.code == "GC-E-024") found_gce024 = true;
    }
    EXPECT_TRUE(found_gce024);

    // Case 3: Composed class disagrees with computed minimum (GC-E-017)
    // claim (V2) depends on dep (V1). Minimum is V1.
    // Declared composed is V2 (which is invalid since V1 is the minimum).
    auto main_claim = make_mock_claim("MAIN-001", VerificationClass::V2);
    main_claim.composition_dependencies.push_back({"DEP-001"});
    
    auto dep_claim = make_mock_claim("DEP-001", VerificationClass::V1);
    
    findings = validate_composed_class(main_claim, {main_claim, dep_claim});
    ASSERT_FALSE(findings.empty());
    bool found_gce017 = false;
    for (const auto& f : findings) {
        if (f.code == "GC-E-017") found_gce017 = true;
    }
    EXPECT_TRUE(found_gce017);
}

// Test validate_claim whole logic (GC-E-019, GC-E-025)
TEST(ValidatorTest, ValidateClaimRules) {
    auto claim = make_mock_claim("RULE-001", VerificationClass::V1);
    std::vector<std::string> team = {"alice", "bob"};

    // "founder" owner always passes GC-E-019
    claim.owner = "founder";
    auto findings = validate_claim(claim, team, {claim});
    EXPECT_TRUE(findings.empty());

    // Member owner present in team list passes
    claim.owner = "alice";
    findings = validate_claim(claim, team, {claim});
    EXPECT_TRUE(findings.empty());

    // Non-member owner fails GC-E-019
    claim.owner = "charlie";
    findings = validate_claim(claim, team, {claim});
    ASSERT_FALSE(findings.empty());
    EXPECT_EQ(findings[0].code, "GC-E-019");

    // Dependency on superseded claim (GC-E-025)
    auto active_claim = make_mock_claim("ACTIVE-001", VerificationClass::V1);
    active_claim.composition_dependencies.push_back({"SUPER-001"});

    auto superseded_claim = make_mock_claim("SUPER-001", VerificationClass::V1);
    superseded_claim.status = ClaimStatus::Superseded;

    findings = validate_claim(active_claim, team, {active_claim, superseded_claim});
    ASSERT_FALSE(findings.empty());
    bool found_gce025 = false;
    for (const auto& f : findings) {
        if (f.code == "GC-E-025") found_gce025 = true;
    }
    EXPECT_TRUE(found_gce025);
}

// Test validate_artifacts logic (GC-E-018)
// Uses an empty file whose SHA-256 is the universally-known constant.
TEST(ValidatorTest, ValidateArtifacts) {
    auto claim = make_mock_claim("ART-001", VerificationClass::V1);
    auto temp_dir = std::filesystem::temp_directory_path();

    // Empty file: SHA-256 is a fixed cryptographic constant.
    auto art_file = temp_dir / "proof_art_empty.txt";
    {
        std::ofstream f(art_file, std::ios::trunc);
        // Write nothing — empty file.
    }

    // SHA-256 of the empty byte sequence (universally known constant).
    const std::string empty_sha =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    ProofArtifact art;
    art.path = "proof_art_empty.txt";
    art.type = "test";
    art.sha256 = empty_sha;
    claim.proof_artifacts.push_back(art);

    // Correct hash, file exists -> passes (no findings)
    auto findings = validate_artifacts(claim, temp_dir);
    EXPECT_TRUE(findings.empty());

    // Hash mismatch -> fails GC-E-018
    claim.proof_artifacts[0].sha256 =
        "0000000000000000000000000000000000000000000000000000000000000000";
    findings = validate_artifacts(claim, temp_dir);
    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].code, "GC-E-018");

    // File missing -> fails GC-E-018
    claim.proof_artifacts[0].path = "nonexistent_file.txt";
    findings = validate_artifacts(claim, temp_dir);
    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].code, "GC-E-018");

    // Clean up
    std::filesystem::remove(art_file);
}

// Test to_string for Severity
TEST(ValidatorTest, SeverityToString) {
    EXPECT_EQ(to_string(Severity::Fatal), "fatal");
    EXPECT_EQ(to_string(Severity::Error), "error");
    EXPECT_EQ(to_string(Severity::Warning), "warning");
    EXPECT_EQ(to_string(static_cast<Severity>(999)), "unknown");
}

// Regression: parse_iso8601 must treat Z-suffix as UTC regardless of the
// workstation's local timezone (GC-E-020 fix for _mkgmtime / timegm).
TEST(ValidatorTest, ParseISO8601IsUTC) {
    auto claim = make_mock_claim("UTC-REG-001", VerificationClass::V1);

    // A well-known past UTC instant: 2020-01-01T00:00:00Z
    // Must always be treated as expired (now > 2020).
    claim.review_expiry = "2020-01-01T00:00:00Z";
    auto findings = validate_expiry(claim);
    ASSERT_EQ(findings.size(), 1u)
        << "2020-01-01T00:00:00Z must always be expired (UTC regression)";
    EXPECT_EQ(findings[0].code, "GC-E-020");

    // A well-known future UTC instant: 2099-12-31T23:59:59Z
    // Must never be treated as expired regardless of local timezone.
    claim.review_expiry = "2099-12-31T23:59:59Z";
    findings = validate_expiry(claim);
    EXPECT_TRUE(findings.empty())
        << "2099-12-31T23:59:59Z must never be expired (UTC regression)";
}

// Test validation for founder ownership passing explicitly (GC-E-019)
TEST(ValidatorTest, FounderOwnerPasses) {
    auto claim = make_mock_claim("FOUNDER-001", VerificationClass::V1);
    claim.owner = "founder";
    std::vector<std::string> team = {}; // Empty team list
    auto findings = validate_claim(claim, team, {claim});
    bool found_gce019 = false;
    for (const auto& f : findings) {
        if (f.code == "GC-E-019") found_gce019 = true;
    }
    EXPECT_FALSE(found_gce019);
}

}  // namespace egbos::govcheck
