// govcheck/src/commands/cmd_validate.cpp
// Spec: S0 Implementation Specification §4.6
#include "commands/cmd_validate.h"
#include "parser/yaml_parser.h"
#include "validator/claim_validator.h"
#include "validator/artifact_validator.h"
#include "validator/ownership_validator.h"
#include "graph/dag_builder.h"
#include "graph/cycle_detector.h"
#include "graph/composition_engine.h"
#include "graph/namespace_checker.h"
#include "reporter/json_reporter.h"
#include <iostream>
#include <algorithm>
#include <regex>
#include <fstream>
#include <cstdlib>

namespace egbos::govcheck {

namespace {

std::vector<ValidationFinding> scan_engine_sources(const std::filesystem::path& repo_root) {
    std::vector<ValidationFinding> findings;
    auto engine_dir = repo_root / "engine";
    if (!std::filesystem::exists(engine_dir) || !std::filesystem::is_directory(engine_dir)) {
        return findings;
    }

    // GC-E-010 patterns — heap allocation/deallocation in engine/ hot path.
    // Pattern design rationale:
    //   re_new:    \bnew\s+\w or \bnew\s*\[ — matches heap allocation expressions;
    //              excludes "new" in comments (already stripped), string literals
    //              (stripped below), and the word "new" used as an adjective.
    //   re_delete: (?<!= )\bdelete\b(?!\s*=) then require operand — match heap
    //              delete but NOT `= delete` (C++11 deleted special members).
    //              Specifically: delete followed by whitespace+expr or '[' (array).
    //   re_malloc / re_free: unchanged — already tight.
    std::regex re_malloc(R"(\bmalloc\s*\()",  std::regex_constants::ECMAScript);
    std::regex re_free  (R"(\bfree\s*\()",    std::regex_constants::ECMAScript);
    // new <Type> or new Type[ — actual heap allocation; excludes adjective "new"
    std::regex re_new   (R"(\bnew\s+(\w|\[))", std::regex_constants::ECMAScript);
    // delete <expr> or delete[] — actual heap deallocation.
    // ECMAScript std::regex has no lookbehind. Instead: match `delete` followed
    // by a space+word or '['; then the caller checks the line does NOT contain
    // `= delete` (C++11 deleted-function syntax). This avoids false positives on
    // `FixedPool(const FixedPool&) = delete;` etc.
    std::regex re_delete(R"(\bdelete\s+(\[|\w))",
                         std::regex_constants::ECMAScript);
    std::regex re_clock (R"(\bCLOCK_REALTIME\b)", std::regex_constants::ECMAScript);

    std::regex re_x_l_tag (R"(\b(order_tag_|x_l_)\b)", std::regex_constants::ECMAScript);
    std::regex re_relaxed (R"(\bmemory_order_relaxed\b)", std::regex_constants::ECMAScript);
    std::regex re_research_include (R"(#\s*include\s*["']research/)", std::regex_constants::ECMAScript);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(engine_dir)) {
        if (!entry.is_regular_file()) continue;
        // Exclude engine/tests — test code is exempt from hot-path rules
        auto rel = std::filesystem::relative(entry.path(), engine_dir);
        if (!rel.empty() && rel.begin()->string() == "tests") continue;
        auto ext = entry.path().extension();
        if (ext == ".cpp" || ext == ".h" || ext == ".cc" || ext == ".hpp") {
            std::ifstream file(entry.path());
            if (!file) continue;

            std::string line;
            int line_num = 0;
            while (std::getline(file, line)) {
                ++line_num;

                // Step 1: strip single-line comments
                auto comment_pos = line.find("//");
                std::string code_line = (comment_pos != std::string::npos)
                                            ? line.substr(0, comment_pos)
                                            : line;

                // Step 2: strip string literals to prevent false positives
                // Replace content inside "..." with spaces (single pass, no nesting).
                {
                    std::string stripped;
                    stripped.reserve(code_line.size());
                    bool in_str = false;
                    for (std::size_t i = 0; i < code_line.size(); ++i) {
                        char c = code_line[i];
                        if (!in_str && c == '"') {
                            in_str = true;
                            stripped += ' ';  // replace opening quote with space
                        } else if (in_str) {
                            if (c == '\\') {
                                ++i;  // skip escaped char
                            } else if (c == '"') {
                                in_str = false;
                            }
                            stripped += ' ';  // replace string content with spaces
                        } else {
                            stripped += c;
                        }
                    }
                    code_line = std::move(stripped);
                }

                if (std::regex_search(code_line, re_malloc) ||
                    std::regex_search(code_line, re_free)   ||
                    std::regex_search(code_line, re_new)    ||
                    std::regex_search(code_line, re_delete)) {
                    findings.push_back({
                        "GC-E-010",
                        Severity::Warning,
                        "",
                        "Prohibited memory allocation/deallocation in engine/ hot path",
                        entry.path().string(),
                        line_num
                    });
                }

                if (std::regex_search(code_line, re_clock)) {
                    findings.push_back({
                        "GC-E-011",
                        Severity::Warning,
                        "",
                        "Prohibited use of CLOCK_REALTIME in engine/",
                        entry.path().string(),
                        line_num
                    });
                }

                if (std::regex_search(code_line, re_research_include)) {
                    findings.push_back({
                        "GC-E-RESEARCH-001",
                        Severity::Error,
                        "",
                        "engine/ imports from research/ namespace (GC-E-RESEARCH-001)",
                        entry.path().string(),
                        line_num
                    });
                }

                if (std::regex_search(code_line, re_relaxed) && code_line.find("CROSS_PLANE") != std::string::npos) {
                    findings.push_back({
                        "GC-E-044", Severity::Error, "",
                        "memory_order_relaxed on cross-plane atomic",
                        entry.path().string(), line_num
                    });
                }

                bool is_pos_engine = (entry.path().filename() == "position_engine.cpp" ||
                                      entry.path().filename() == "position_engine.h" ||
                                      entry.path().filename() == "cross_plane.h");

                if (!is_pos_engine && std::regex_search(code_line, re_x_l_tag)) {
                    findings.push_back({
                        "GC-E-045", Severity::Error, "", // GC-E-043 / GC-E-045
                        "Direct access to order_tag_ or x_l_ outside L3 without CROSS_PLANE_READ",
                        entry.path().string(), line_num
                    });
                }
            }
        }
    }
    return findings;
}

} // namespace

int cmd_validate(const std::string& scope,
                 bool strict,
                 const std::filesystem::path& repo_root)
{
    std::vector<ValidationFinding> all_findings;

    // 1. Load all claims
    auto claims_dir = repo_root / "governance" / "claims";
    auto parse_results = load_claims_from_dir(claims_dir);

    std::vector<ParsedClaim> claims;
    for (auto& pr : parse_results) {
        if (!pr.ok()) {
            for (auto& e : pr.errors) {
                all_findings.push_back({
                    "SCHEMA-PARSE-ERROR",
                    Severity::Error,
                    "",
                    e.message,
                    e.file,
                    e.line
                });
            }
        } else {
            claims.push_back(std::move(*pr.claim));
        }
    }

    // GC-F-002: Verify version mismatch if env var is set
    const char* expected_ver = std::getenv("GOVCHECK_EXPECTED_VERSION");
    if (expected_ver && std::string(expected_ver) != "1.0.0") {
        all_findings.push_back({
            "GC-F-002", Severity::Fatal, "",
            "govcheck binary version mismatch with CI: expected=" + std::string(expected_ver) + " actual=1.0.0",
            "", 0
        });
    }

    // GC-F-004: Verify CLAIM-GOVCHECK-001 exists, is active, and is at V4
    bool govcheck_claim_found = false;
    for (const auto& claim : claims) {
        if (claim.claim_id == "CLAIM-GOVCHECK-001") {
            govcheck_claim_found = true;
            if (claim.status != ClaimStatus::Active) {
                all_findings.push_back({
                    "GC-F-004", Severity::Fatal, "CLAIM-GOVCHECK-001",
                    "CLAIM-GOVCHECK-001 is not active",
                    claim.source_file.string(), 0
                });
            }
            if (claim.verification_class != VerificationClass::V4 ||
                claim.composed_verification_class != VerificationClass::V4) {
                all_findings.push_back({
                    "GC-F-004", Severity::Fatal, "CLAIM-GOVCHECK-001",
                    "CLAIM-GOVCHECK-001 verification class is not V4",
                    claim.source_file.string(), 0
                });
            }
            if (claim.proof_artifacts.empty()) {
                all_findings.push_back({
                    "GC-F-004", Severity::Fatal, "CLAIM-GOVCHECK-001",
                    "CLAIM-GOVCHECK-001 has no proof artifacts",
                    claim.source_file.string(), 0
                });
            } else {
                const char* env_hwid = std::getenv("EGBOS_HARDWARE_ID");
                if (env_hwid) {
                    std::string hwid(env_hwid);
                    bool hwid_matched = false;
                    for (const auto& art : claim.proof_artifacts) {
                        if (art.path.find(hwid) != std::string::npos) {
                            hwid_matched = true;
                            break;
                        }
                    }
                    if (!hwid_matched) {
                        all_findings.push_back({
                            "GC-F-004", Severity::Fatal, "CLAIM-GOVCHECK-001",
                            "CLAIM-GOVCHECK-001 has no proof artifact for current hardware_id: " + hwid,
                            claim.source_file.string(), 0
                        });
                    }
                }
            }
            break;
        }
    }
    if (!govcheck_claim_found) {
        all_findings.push_back({
            "GC-F-004", Severity::Fatal, "",
            "CLAIM-GOVCHECK-001 (govcheck self-verification) is missing",
            "", 0
        });
    }

    // 2. Load team members
    auto team_file = repo_root / "governance" / "team.yaml";
    std::vector<std::string> team_members = load_team_members(team_file);
    if (team_members.empty()) {
        ValidationFinding f;
        f.code = "TEAM-LOAD-ERROR";
        f.severity = Severity::Fatal;
        f.claim_id = "";
        f.message = "Failed to load team members from: " + team_file.string();
        f.file = team_file.string();
        f.line = 0;
        all_findings.push_back(std::move(f));
        // Fatal — cannot continue without team registry
        ReportSummary summary;
        summary.fatals = static_cast<int>(all_findings.size());
        summary.passed = false;
        std::cout << build_report(scope, summary, all_findings) << "\n";
        return 2;
    }

    // 3. Per-claim validation
    for (auto& claim : claims) {
        auto findings = validate_claim(claim, team_members, claims);
        all_findings.insert(all_findings.end(), findings.begin(), findings.end());

        auto artifact_findings = validate_artifacts(claim, repo_root);
        all_findings.insert(all_findings.end(), artifact_findings.begin(), artifact_findings.end());
    }

    // 4. DAG + cycle detection (GC-F-001)
    auto dag = build_dag(claims);
    auto cycle_findings = detect_cycles(dag);
    all_findings.insert(all_findings.end(), cycle_findings.begin(), cycle_findings.end());

    // 5. Composition engine (GC-E-017, GC-E-024)
    auto comp_findings = verify_composed_classes(claims, dag);
    all_findings.insert(all_findings.end(), comp_findings.begin(), comp_findings.end());

    // 6. Namespace checker (GC-E-023, GC-F-003)
    auto ns_deps = load_namespace_deps(repo_root);
    auto ns_findings = check_namespace_deps(claims, ns_deps);
    all_findings.insert(all_findings.end(), ns_findings.begin(), ns_findings.end());

    // 6b. Engine source code scanning (GC-E-010, GC-E-011)
    auto engine_findings = scan_engine_sources(repo_root);
    all_findings.insert(all_findings.end(), engine_findings.begin(), engine_findings.end());

    // 7. Promote warnings to errors in strict mode
    // GC-E-010 and GC-E-011 are activation-phase-gated (S3 and S2 respectively).
    // They must NOT be promoted by --strict until their activation slice.
    if (strict) {
        for (auto& f : all_findings) {
            if (f.severity == Severity::Warning &&
                f.code != "GC-E-010" && f.code != "GC-E-011") {
                f.severity = Severity::Error;
            }
        }
    }

    // 8. Build summary
    ReportSummary summary;
    summary.total_claims = static_cast<int>(claims.size());
    for (auto& f : all_findings) {
        if (f.severity == Severity::Fatal)   ++summary.fatals;
        else if (f.severity == Severity::Error)   ++summary.errors;
        else if (f.severity == Severity::Warning) ++summary.warnings;
    }
    summary.passed = (summary.fatals == 0 && summary.errors == 0);

    std::cout << build_report(scope, summary, all_findings) << "\n";

    if (summary.fatals > 0) return 2;
    if (summary.errors > 0) return 1;
    return 0;
}

} // namespace egbos::govcheck
