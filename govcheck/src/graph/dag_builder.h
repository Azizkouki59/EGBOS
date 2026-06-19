// govcheck/src/graph/dag_builder.h
// Build claim dependency DAG from composition_dependencies.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "parser/yaml_parser.h"

namespace egbos::govcheck {

// Adjacency list: claim_id -> list of claim_ids it depends on
using ClaimGraph = std::unordered_map<std::string, std::vector<std::string>>;

// Build directed graph from all parsed claims.
// Edge: A -> B means A depends on B.
ClaimGraph build_dag(const std::vector<ParsedClaim>& claims);

}  // namespace egbos::govcheck
