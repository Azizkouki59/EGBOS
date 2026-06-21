// govcheck/tests/test_reporter.cpp
// GoogleTest: JSON report generation
// Spec: S0 Implementation Specification §7.1, §4.5

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "reporter/json_reporter.h"
#include "validator/claim_validator.h"

namespace egbos::govcheck {

// ── build_report: empty findings ─────────────────────────────────────────────

TEST(ReporterTest, EmptyFindingsReport) {
    ReportSummary summary;
    summary.total_claims = 5;
    summary.fatals = 0;
    summary.errors = 0;
    summary.warnings = 0;
    summary.passed = true;

    std::string json_str = build_report("C0", summary, {});
    ASSERT_FALSE(json_str.empty());

    nlohmann::json doc = nlohmann::json::parse(json_str);

    EXPECT_EQ(doc["govcheck_version"].get<std::string>(), "1.0.0");
    EXPECT_EQ(doc["scope"].get<std::string>(), "C0");
    EXPECT_TRUE(doc.contains("timestamp"));

    EXPECT_EQ(doc["summary"]["total_claims"].get<int>(), 5);
    EXPECT_EQ(doc["summary"]["fatals"].get<int>(), 0);
    EXPECT_EQ(doc["summary"]["errors"].get<int>(), 0);
    EXPECT_EQ(doc["summary"]["warnings"].get<int>(), 0);
    EXPECT_TRUE(doc["summary"]["passed"].get<bool>());

    EXPECT_TRUE(doc["findings"].is_array());
    EXPECT_EQ(doc["findings"].size(), 0u);
}

// ── build_report: single finding fields ──────────────────────────────────────

TEST(ReporterTest, SingleFindingReport) {
    ReportSummary summary;
    summary.total_claims = 3;
    summary.fatals = 0;
    summary.errors = 1;
    summary.warnings = 0;
    summary.passed = false;

    ValidationFinding finding;
    finding.code = "GC-E-018";
    finding.severity = Severity::Error;
    finding.claim_id = "CLAIM-001";
    finding.message = "Proof artifact SHA-256 mismatch";
    finding.file = "governance/claims/CLAIM-001.yaml";
    finding.line = 0;

    std::string json_str = build_report("C0", summary, {finding});
    nlohmann::json doc = nlohmann::json::parse(json_str);

    EXPECT_FALSE(doc["summary"]["passed"].get<bool>());
    EXPECT_EQ(doc["summary"]["errors"].get<int>(), 1);

    ASSERT_EQ(doc["findings"].size(), 1u);
    const auto& f = doc["findings"][0];
    EXPECT_EQ(f["code"].get<std::string>(), "GC-E-018");
    EXPECT_EQ(f["severity"].get<std::string>(), "error");
    EXPECT_EQ(f["claim_id"].get<std::string>(), "CLAIM-001");
    EXPECT_EQ(f["message"].get<std::string>(), "Proof artifact SHA-256 mismatch");
    EXPECT_EQ(f["file"].get<std::string>(), "governance/claims/CLAIM-001.yaml");
    EXPECT_EQ(f["line"].get<int>(), 0);
}

// ── build_report: severity to_string mapping ─────────────────────────────────

TEST(ReporterTest, SeverityStrings) {
    auto make_finding = [](Severity s) -> ValidationFinding {
        return {"GC-F-001", s, "X", "msg", "f.yaml", 0};
    };

    ReportSummary summary;
    summary.total_claims = 1;
    summary.passed = false;

    {
        auto json_str = build_report("C0", summary, {make_finding(Severity::Fatal)});
        auto doc = nlohmann::json::parse(json_str);
        EXPECT_EQ(doc["findings"][0]["severity"].get<std::string>(), "fatal");
    }
    {
        auto json_str = build_report("C0", summary, {make_finding(Severity::Warning)});
        auto doc = nlohmann::json::parse(json_str);
        EXPECT_EQ(doc["findings"][0]["severity"].get<std::string>(), "warning");
    }
}

// ── build_report: multiple findings preserved ────────────────────────────────

TEST(ReporterTest, MultipleFindingsPreservedOrder) {
    ReportSummary summary;
    summary.total_claims = 10;
    summary.fatals = 1;
    summary.errors = 2;
    summary.warnings = 1;
    summary.passed = false;

    std::vector<ValidationFinding> findings = {
        {"GC-F-001", Severity::Fatal,   "A", "Cycle detected",    "a.yaml", 0},
        {"GC-E-019", Severity::Error,   "B", "Owner not in team", "b.yaml", 0},
        {"GC-E-020", Severity::Warning, "C", "Review expired",    "c.yaml", 0},
        {"GC-E-023", Severity::Error,   "D", "Cross-ns dep",      "d.yaml", 0},
    };

    std::string json_str = build_report("C0", summary, findings);
    auto doc = nlohmann::json::parse(json_str);

    ASSERT_EQ(doc["findings"].size(), 4u);
    EXPECT_EQ(doc["findings"][0]["code"].get<std::string>(), "GC-F-001");
    EXPECT_EQ(doc["findings"][1]["code"].get<std::string>(), "GC-E-019");
    EXPECT_EQ(doc["findings"][2]["code"].get<std::string>(), "GC-E-020");
    EXPECT_EQ(doc["findings"][3]["code"].get<std::string>(), "GC-E-023");

    EXPECT_EQ(doc["summary"]["fatals"].get<int>(), 1);
    EXPECT_EQ(doc["summary"]["errors"].get<int>(), 2);
    EXPECT_EQ(doc["summary"]["warnings"].get<int>(), 1);
}

}  // namespace egbos::govcheck
