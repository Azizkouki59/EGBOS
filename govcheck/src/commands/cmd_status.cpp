// govcheck/src/commands/cmd_status.cpp
// Spec: S0 Implementation Specification §4.6
#include "commands/cmd_status.h"
#include "parser/yaml_parser.h"
#include "reporter/json_reporter.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <map>

namespace egbos::govcheck {

int cmd_status(const std::filesystem::path& repo_root)
{
    auto claims_dir = repo_root / "governance" / "claims";
    auto parse_results = load_claims_from_dir(claims_dir);

    std::map<std::string, int> by_status;
    std::map<std::string, int> by_class;
    std::map<std::string, int> by_namespace;
    int total = 0;
    int parse_errors = 0;

    for (auto& pr : parse_results) {
        if (!pr.ok()) { ++parse_errors; continue; }
        ++total;
        by_status[to_string(pr.claim->status)]++;
        by_class[to_string(pr.claim->verification_class)]++;
        by_namespace[pr.claim->namespace_id]++;
    }

    nlohmann::json out;
    out["govcheck_version"] = "1.0.0";
    out["total_claims"]     = total;
    out["parse_errors"]     = parse_errors;
    out["by_status"]        = by_status;
    out["by_verification_class"] = by_class;
    out["by_namespace"]     = by_namespace;

    std::cout << out.dump(2) << "\n";
    return 0;
}

} // namespace egbos::govcheck
