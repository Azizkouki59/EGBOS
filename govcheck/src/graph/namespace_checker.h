// govcheck/src/graph/namespace_checker.h
// Enforces GC-E-023: undeclared cross-namespace dependency.
// Enforces GC-F-003: namespace-deps.yaml malformed.
//
// Authority: EGBOS_MASTER_augmented.html §B1.3.3, §B1.8
// Spec: S0 Implementation Specification §4.4.4

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../parser/claim_schema.h"
#include "../validator/claim_validator.h"

namespace egbos::govcheck {

// Allowed dependency graph for namespaces.
// Key: namespace name. Value: set of namespaces it may depend on.
using NamespaceDepsGraph =
    std::unordered_map<std::string, std::unordered_set<std::string>>;

// Result of loading namespace-deps.yaml.
struct NamespaceDepsLoadResult {
    bool ok = false;
    NamespaceDepsGraph graph;
    std::string error_message;  // populated when ok == false (GC-F-003)
};

// Load and parse governance/namespace-deps.yaml.
// Returns GC-F-003 error via ok==false if file is missing or malformed.
NamespaceDepsLoadResult load_namespace_deps(
    const std::filesystem::path& repo_root);

// Check all claims for undeclared cross-namespace dependencies.
// Emits GC-E-023 for each undeclared cross-namespace edge.
// Emits GC-F-003 (fatal) if namespace-deps graph failed to load.
std::vector<ValidationFinding> check_namespace_deps(
    const std::vector<ParsedClaim>& claims,
    const NamespaceDepsLoadResult& deps);

// Extract the top-level namespace prefix from a dot-separated namespace string.
// e.g. "exec.fix_consumer" -> "exec"
//      "bootstrap"         -> "bootstrap"
std::string namespace_prefix(const std::string& ns);

}  // namespace egbos::govcheck
