// govcheck/src/graph/composition_engine.cpp

#include "composition_engine.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace egbos::govcheck {

static int vc_rank(VerificationClass vc) {
    switch (vc) {
        case VerificationClass::V1: return 1;
        case VerificationClass::V2: return 2;
        case VerificationClass::V3: return 3;
        case VerificationClass::V4: return 4;
        default:                    return 0;
    }
}

static VerificationClass rank_to_vc(int rank) {
    switch (rank) {
        case 1: return VerificationClass::V1;
        case 2: return VerificationClass::V2;
        case 3: return VerificationClass::V3;
        case 4: return VerificationClass::V4;
        default: return VerificationClass::Unknown;
    }
}

// Recursively compute minimum class across all transitive dependencies
static int compute_min_rank(
    const std::string& claim_id,
    const std::unordered_map<std::string, const ParsedClaim*>& claim_map,
    const ClaimGraph& graph,
    std::unordered_set<std::string>& visited) {

    if (visited.count(claim_id)) return 4;  // already visited, avoid infinite loop
    visited.insert(claim_id);

    auto it = claim_map.find(claim_id);
    if (it == claim_map.end()) return 4;  // unknown dep — don't penalise

    int min_rank = vc_rank(it->second->verification_class);

    auto graph_it = graph.find(claim_id);
    if (graph_it != graph.end()) {
        for (const auto& dep_id : graph_it->second) {
            int dep_rank = compute_min_rank(dep_id, claim_map, graph, visited);
            if (dep_rank < min_rank) min_rank = dep_rank;
        }
    }

    return min_rank;
}

std::vector<ValidationFinding> verify_composed_classes(
    const std::vector<ParsedClaim>& claims,
    const ClaimGraph& graph) {

    std::vector<ValidationFinding> findings;

    // Build lookup map
    std::unordered_map<std::string, const ParsedClaim*> claim_map;
    for (const auto& claim : claims) {
        claim_map[claim.claim_id] = &claim;
    }

    for (const auto& claim : claims) {
        std::unordered_set<std::string> visited;
        int computed_min = compute_min_rank(claim.claim_id, claim_map, graph, visited);
        VerificationClass computed_vc = rank_to_vc(computed_min);

        int declared_rank = vc_rank(claim.composed_verification_class);
        int own_rank      = vc_rank(claim.verification_class);

        // GC-E-024: composed class exceeds own declared class
        if (declared_rank > own_rank) {
            findings.push_back(
                {"GC-E-024", Severity::Error, claim.claim_id,
                 "composed_verification_class (" +
                     to_string(claim.composed_verification_class) +
                     ") exceeds verification_class (" +
                     to_string(claim.verification_class) + ")",
                 claim.source_file.string(), 0});
        }

        // GC-E-017: declared composed class disagrees with computed minimum
        if (declared_rank != computed_min) {
            findings.push_back(
                {"GC-E-017", Severity::Error, claim.claim_id,
                 "composed_verification_class (" +
                     to_string(claim.composed_verification_class) +
                     ") does not match computed minimum (" +
                     to_string(computed_vc) + ")",
                 claim.source_file.string(), 0});
        }
    }

    return findings;
}

}  // namespace egbos::govcheck
