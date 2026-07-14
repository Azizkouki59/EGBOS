// govcheck/src/commands/cmd_scan.h
// govcheck scan --file <path>
// §15.4 S1 source-level governance rule scanner.
// Implements: GC-E-054, GC-E-056, GC-E-SECRETS-001 (source scan)
//             GC-E-053 (companion claim YAML check)
// Spec: Master Blueprint §I7.2 GC-E Rule Activation Schedule

#pragma once
#include <filesystem>

namespace egbos::govcheck {

// Scan a single file for source-level governance rule violations.
// For .cpp companion files (e.g. violation_GC_E_053.cpp), also checks any
// matching fixture_*_claim.yaml in the same directory for claim-level rules.
//
// Prints a JSON report to stdout.
// Exit codes: 0 = no violations, 1 = violations found,
//             2 = fatal, 3 = usage/IO error.
int cmd_scan(const std::filesystem::path& file_path);

}  // namespace egbos::govcheck
