// govcheck/src/commands/cmd_validate.h
// govcheck validate --scope <scope> [--strict] [--repo-root <path>]
// Spec: S0 Implementation Specification §4.6
#pragma once
#include <filesystem>
#include <string>

namespace egbos::govcheck {

// Exit codes per spec §4.6
// 0 = pass, 1 = errors, 2 = fatal, 3 = usage error
int cmd_validate(const std::string& scope,
                 bool strict,
                 const std::filesystem::path& repo_root);

} // namespace egbos::govcheck
