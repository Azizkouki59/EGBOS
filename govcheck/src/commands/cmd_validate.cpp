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

namespace egbos::govcheck {

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

    // 2. Load team members
    auto team_file = repo_root / "governance" / "team.yaml";
    auto team_result = load_team_members(team_file);
    if (!team_result.errors.empty()) {
        for (auto& e : team_result.errors) {
            all_findings.push_back({
                "TEAM-LOAD-ERROR",
                Severity::Fatal,
                "",
                e,
                team_file.string(),
                0
            });
        }
        // Fatal — cannot continue without team registry
        ReportSummary summary;
        summary.fatals = static_cast<int>(all_findings.size());
        summary.passed = false;
        std::cout << build_report(scope, summary, all_findings) << "\n";
        return 2;
    }

    // 3. Per-claim validation
    for (auto& claim : claims) {
        auto findings = validate_claim(claim, team_result.members, claims);
        all_findings.insert(all_findings.end(), findings.begin(), findings.end());

        auto expiry_findings = validate_expiry(claim);
        all_findings.insert(all_findings.end(), expiry_findings.begin(), expiry_findings.end());

        auto composed_findings = validate_composed_class(claim, claims);
        all_findings.insert(all_findings.end(), composed_findings.begin(), composed_findings.end());

        auto artifact_findings = validate_artifacts(claim, repo_root);
        all_findings.insert(all_findings.end(), artifact_findings.begin(), artifact_findings.end());
    }

    // 4. DAG + cycle detection (GC-F-001)
    auto dag = build_dag(claims);
    auto cycle_findings = detect_cycles(dag, claims);
    all_findings.insert(all_findings.end(), cycle_findings.begin(), cycle_findings.end());

    // 5. Composition engine (GC-E-017, GC-E-024)
    auto comp_findings = check_composition(dag, claims);
    all_findings.insert(all_findings.end(), comp_findings.begin(), comp_findings.end());

    // 6. Namespace checker (GC-E-023, GC-F-003)
    auto ns_file = repo_root / "governance" / "namespace-deps.yaml";
    auto ns_findings = check_namespaces(claims, ns_file);
    all_findings.insert(all_findings.end(), ns_findings.begin(), ns_findings.end());

    // 7. Promote warnings to errors in strict mode
    if (strict) {
        for (auto& f : all_findings) {
            if (f.severity == Severity::Warning) {
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
