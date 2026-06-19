// govcheck/src/graph/namespace_checker.cpp
// GC-E-023: undeclared cross-namespace dependency.
// GC-F-003: namespace-deps.yaml malformed or missing.
//
// Authority: EGBOS_MASTER_augmented.html §B1.3.3, §B1.8
// Spec: S0 Implementation Specification §4.4.4

#include "namespace_checker.h"

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <stdexcept>

namespace egbos::govcheck {

// ── namespace_prefix ─────────────────────────────────────────────────────────

std::string namespace_prefix(const std::string& ns) {
    auto pos = ns.find('.');
    if (pos == std::string::npos) return ns;
    return ns.substr(0, pos);
}

// ── load_namespace_deps ───────────────────────────────────────────────────────

NamespaceDepsLoadResult load_namespace_deps(
    const std::filesystem::path& repo_root) {

    NamespaceDepsLoadResult result;
    auto path = repo_root / "governance" / "namespace-deps.yaml";

    if (!std::filesystem::exists(path)) {
        result.ok = false;
        result.error_message =
            "namespace-deps.yaml not found at: " + path.string();
        return result;
    }

    try {
        YAML::Node root = YAML::LoadFile(path.string());

        // GC-F-003: must be a map with a 'namespaces' key
        if (!root || !root.IsMap()) {
            result.ok = false;
            result.error_message =
                "namespace-deps.yaml is not a YAML map: " + path.string();
            return result;
        }

        if (!root["namespaces"] || !root["namespaces"].IsMap()) {
            result.ok = false;
            result.error_message =
                "namespace-deps.yaml missing required 'namespaces' map key";
            return result;
        }

        for (const auto& entry : root["namespaces"]) {
            std::string ns_name = entry.first.as<std::string>();
            const YAML::Node& ns_node = entry.second;

            if (!ns_node || !ns_node.IsMap()) {
                result.ok = false;
                result.error_message =
                    "namespace '" + ns_name +
                    "' in namespace-deps.yaml is not a map";
                return result;
            }

            std::unordered_set<std::string> allowed;

            if (ns_node["allowed_dependencies"]) {
                const YAML::Node& deps = ns_node["allowed_dependencies"];
                if (!deps.IsSequence()) {
                    result.ok = false;
                    result.error_message =
                        "namespace '" + ns_name +
                        "': 'allowed_dependencies' must be a sequence";
                    return result;
                }
                for (const auto& dep : deps) {
                    allowed.insert(dep.as<std::string>());
                }
            }

            result.graph[ns_name] = std::move(allowed);
        }

        result.ok = true;

    } catch (const YAML::Exception& e) {
        result.ok = false;
        result.error_message =
            std::string("namespace-deps.yaml parse error (GC-F-003): ") +
            e.what();
    } catch (const std::exception& e) {
        result.ok = false;
        result.error_message =
            std::string("namespace-deps.yaml load error: ") + e.what();
    }

    return result;
}

// ── check_namespace_deps ──────────────────────────────────────────────────────

std::vector<ValidationFinding> check_namespace_deps(
    const std::vector<ParsedClaim>& claims,
    const NamespaceDepsLoadResult& deps) {

    std::vector<ValidationFinding> findings;

    // GC-F-003: fatal if namespace-deps failed to load
    if (!deps.ok) {
        findings.push_back(
            {"GC-F-003", Severity::Fatal, "",
             "namespace-deps.yaml malformed or missing: " + deps.error_message,
             "governance/namespace-deps.yaml", 0});
        return findings;
    }

    for (const auto& claim : claims) {
        std::string claim_ns_prefix = namespace_prefix(claim.ns);

        for (const auto& dep : claim.composition_dependencies) {
            // Find the dependency claim's namespace
            // We need to resolve dep.claim_id to its namespace.
            // Approach: search all_claims for the dep claim_id.
            // Since we only have claims here, we look it up inline.
            // (The dep struct carries claim_id; we resolve via the claims list.)
            std::string dep_ns_prefix;
            bool dep_found = false;

            for (const auto& other : claims) {
                if (other.claim_id == dep.claim_id) {
                    dep_ns_prefix = namespace_prefix(other.ns);
                    dep_found = true;
                    break;
                }
            }

            if (!dep_found) {
                // Unresolved dependency — reported by dag_builder, not here
                continue;
            }

            // Same namespace prefix — no cross-namespace check needed
            if (dep_ns_prefix == claim_ns_prefix) continue;

            // Cross-namespace dependency: check it is declared
            auto it = deps.graph.find(claim_ns_prefix);
            if (it == deps.graph.end()) {
                // Claim's namespace is not registered in namespace-deps.yaml
                findings.push_back(
                    {"GC-E-023", Severity::Error, claim.claim_id,
                     "Claim namespace '" + claim_ns_prefix +
                         "' is not registered in namespace-deps.yaml",
                     claim.source_file.string(), 0});
                continue;
            }

            const auto& allowed = it->second;
            if (allowed.find(dep_ns_prefix) == allowed.end()) {
                findings.push_back(
                    {"GC-E-023", Severity::Error, claim.claim_id,
                     "Undeclared cross-namespace dependency: '" +
                         claim_ns_prefix + "' -> '" + dep_ns_prefix +
                         "' (dependency claim: " + dep.claim_id + ")",
                     claim.source_file.string(), 0});
            }
        }
    }

    return findings;
}

}  // namespace egbos::govcheck
