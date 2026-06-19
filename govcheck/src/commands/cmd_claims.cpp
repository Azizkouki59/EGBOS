// govcheck/src/commands/cmd_claims.cpp
// Spec: S0 Implementation Specification §4.6
#include "commands/cmd_claims.h"
#include "parser/yaml_parser.h"
#include <nlohmann/json.hpp>
#include <iostream>

namespace egbos::govcheck {

int cmd_claims(const std::string& filter_namespace,
               const std::string& filter_status,
               const std::filesystem::path& repo_root)
{
    auto claims_dir = repo_root / "governance" / "claims";
    auto parse_results = load_claims_from_dir(claims_dir);

    nlohmann::json arr = nlohmann::json::array();
    for (auto& pr : parse_results) {
        if (!pr.ok()) continue;
        const auto& c = *pr.claim;

        if (!filter_namespace.empty() && c.namespace_id != filter_namespace) continue;
        if (!filter_status.empty()    && to_string(c.status) != filter_status)  continue;

        arr.push_back({
            {"claim_id",           c.claim_id},
            {"namespace",          c.namespace_id},
            {"verification_class", to_string(c.verification_class)},
            {"status",             to_string(c.status)},
            {"owner",              c.owner}
        });
    }

    std::cout << arr.dump(2) << "\n";
    return 0;
}

} // namespace egbos::govcheck
