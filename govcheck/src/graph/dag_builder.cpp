// govcheck/src/graph/dag_builder.cpp

#include "dag_builder.h"

namespace egbos::govcheck {

ClaimGraph build_dag(const std::vector<ParsedClaim>& claims) {
    ClaimGraph graph;

    // Ensure every claim has an entry (even if no dependencies)
    for (const auto& claim : claims) {
        graph[claim.claim_id];  // default-initialize empty vector
    }

    // Add edges: claim -> dependency
    for (const auto& claim : claims) {
        for (const auto& dep : claim.composition_dependencies) {
            graph[claim.claim_id].push_back(dep.claim_id);
        }
    }

    return graph;
}

}  // namespace egbos::govcheck
