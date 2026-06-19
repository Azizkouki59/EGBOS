// govcheck/src/commands/cmd_claims.h
// govcheck claims [--namespace <ns>] [--status <status>] [--repo-root <path>]
// Spec: S0 Implementation Specification §4.6
#pragma once
#include <filesystem>
#include <string>

namespace egbos::govcheck {

// Always exits 0 (informational).
int cmd_claims(const std::string& filter_namespace,
               const std::string& filter_status,
               const std::filesystem::path& repo_root);

} // namespace egbos::govcheck
