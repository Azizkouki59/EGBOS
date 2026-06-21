// govcheck/tests/test_graph.cpp
// GoogleTest: Dependency graphs, cycles, composition, and namespaces
// Spec: S0 Implementation Specification §7.1

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "graph/dag_builder.h"
#include "graph/cycle_detector.h"
#include "graph/composition_engine.h"
#include "graph/namespace_checker.h"
#include "parser/yaml_parser.h"

namespace egbos::govcheck {
namespace {

ParsedClaim make_mock_claim(const std::string& id, VerificationClass vc, const std::string& ns = "bootstrap") {
    ParsedClaim c;
    c.claim_id = id;
    c.verification_class = vc;
    c.composed_verification_class = vc;
    c.namespace_id = ns;
    c.status = ClaimStatus::Active;
    c.source_file = "claims/" + id + ".yaml";
    return c;
}

}  // namespace

// ── DAG Builder Tests ────────────────────────────────────────────────────────

TEST(GraphTest, BuildDagBasic) {
    std::vector<ParsedClaim> claims;
    
    auto c1 = make_mock_claim("A", VerificationClass::V2);
    CompositionDependency dep1{"B"};
    c1.composition_dependencies.push_back(dep1);
    
    auto c2 = make_mock_claim("B", VerificationClass::V1);

    claims.push_back(c1);
    claims.push_back(c2);

    ClaimGraph graph = build_dag(claims);

    ASSERT_EQ(graph.size(), 2u);
    EXPECT_TRUE(graph.count("A"));
    EXPECT_TRUE(graph.count("B"));

    EXPECT_EQ(graph["A"].size(), 1u);
    if (!graph["A"].empty()) {
        EXPECT_EQ(graph["A"][0], "B");
    }
    EXPECT_TRUE(graph["B"].empty());
}

// ── Cycle Detector Tests ─────────────────────────────────────────────────────

TEST(GraphTest, CycleDetectorNoCycles) {
    ClaimGraph graph;
    graph["A"] = {"B"};
    graph["B"] = {"C"};
    graph["C"] = {};

    auto findings = detect_cycles(graph);
    EXPECT_TRUE(findings.empty());
}

TEST(GraphTest, CycleDetectorSelfLoop) {
    ClaimGraph graph;
    graph["A"] = {"A"};

    auto findings = detect_cycles(graph);
    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].code, "GC-F-001");
    EXPECT_EQ(findings[0].severity, Severity::Fatal);
}

TEST(GraphTest, CycleDetectorMutualCycle) {
    ClaimGraph graph;
    graph["A"] = {"B"};
    graph["B"] = {"A"};

    auto findings = detect_cycles(graph);
    ASSERT_FALSE(findings.empty());
    EXPECT_EQ(findings[0].code, "GC-F-001");
    EXPECT_EQ(findings[0].severity, Severity::Fatal);
}

// ── Composition Engine Tests ─────────────────────────────────────────────────

TEST(GraphTest, CompositionEngineValidation) {
    // Valid composition
    std::vector<ParsedClaim> claims;
    auto c1 = make_mock_claim("A", VerificationClass::V2);
    c1.composition_dependencies.push_back({"B"});
    
    auto c2 = make_mock_claim("B", VerificationClass::V1);
    
    claims.push_back(c1);
    claims.push_back(c2);

    ClaimGraph graph = build_dag(claims);

    // If declared composed class of A is V2, it fails because dependency B is V1 (computed min = V1)
    // Here c1 has composed_verification_class = V2 (defaulted in make_mock_claim)
    auto findings = verify_composed_classes(claims, graph);
    ASSERT_FALSE(findings.empty());
    EXPECT_EQ(findings[0].code, "GC-E-017");

    // Correcting declared class to V1 passes
    claims[0].composed_verification_class = VerificationClass::V1;
    findings = verify_composed_classes(claims, graph);
    EXPECT_TRUE(findings.empty());

    // Exceeding own verification class (GC-E-024)
    claims[0].verification_class = VerificationClass::V1;
    claims[0].composed_verification_class = VerificationClass::V2;
    findings = verify_composed_classes(claims, graph);
    ASSERT_FALSE(findings.empty());
    bool found_gce024 = false;
    for (const auto& f : findings) {
        if (f.code == "GC-E-024") found_gce024 = true;
    }
    EXPECT_TRUE(found_gce024);
}

// ── Namespace Checker Tests ──────────────────────────────────────────────────

TEST(GraphTest, NamespacePrefixUtility) {
    EXPECT_EQ(namespace_prefix("exec.fix_consumer"), "exec");
    EXPECT_EQ(namespace_prefix("bootstrap"), "bootstrap");
    EXPECT_EQ(namespace_prefix(""), "");
}

TEST(GraphTest, NamespaceCheckerLoadAndParse) {
    auto temp_root = std::filesystem::temp_directory_path() / "test_graph_temp";
    std::filesystem::create_directories(temp_root / "governance");

    auto yaml_path = temp_root / "governance" / "namespace-deps.yaml";
    
    // Write valid namespace-deps
    {
        std::ofstream f(yaml_path, std::ios::trunc);
        f << "namespaces:\n"
             "  bootstrap:\n"
             "    allowed_dependencies: []\n"
             "  exec:\n"
             "    allowed_dependencies:\n"
             "      - bootstrap\n";
    }

    auto res = load_namespace_deps(temp_root);
    EXPECT_TRUE(res.ok);
    EXPECT_TRUE(res.error_message.empty());
    EXPECT_EQ(res.graph.size(), 2u);
    EXPECT_TRUE(res.graph.count("bootstrap"));
    EXPECT_TRUE(res.graph.count("exec"));
    EXPECT_TRUE(res.graph["exec"].count("bootstrap"));

    // Write malformed/invalid namespace-deps (missing namespaces key)
    {
        std::ofstream f(yaml_path, std::ios::trunc);
        f << "invalid_key:\n"
             "  bootstrap: []\n";
    }

    auto res_invalid = load_namespace_deps(temp_root);
    EXPECT_FALSE(res_invalid.ok);
    EXPECT_NE(res_invalid.error_message.find("missing required 'namespaces'"),
              std::string::npos);

    // Clean up
    std::filesystem::remove_all(temp_root);
}

TEST(GraphTest, NamespaceDepsCheckerErrors) {
    // If NamespaceDepsLoadResult is marked not OK, check_namespace_deps emits GC-F-003
    NamespaceDepsLoadResult bad_res;
    bad_res.ok = false;
    bad_res.error_message = "Mock file missing";

    std::vector<ParsedClaim> claims = { make_mock_claim("A", VerificationClass::V1) };
    auto findings = check_namespace_deps(claims, bad_res);
    ASSERT_EQ(findings.size(), 1u);
    EXPECT_EQ(findings[0].code, "GC-F-003");
    EXPECT_EQ(findings[0].severity, Severity::Fatal);
}

}  // namespace egbos::govcheck
