// govcheck/src/commands/cmd_status.h
// govcheck status [--repo-root <path>]
// Spec: S0 Implementation Specification §4.6
#pragma once
#include <filesystem>

namespace egbos::govcheck {

// Always exits 0 (informational).
int cmd_status(const std::filesystem::path& repo_root);

} // namespace egbos::govcheck
