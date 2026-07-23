// govcheck/src/main.cpp
// govcheck — CLI entry point and command dispatch.
// Spec: S0 Implementation Specification §4.6, extended per §I2 for S1+ composite scopes
//
// Usage:
//   govcheck validate --scope <scope> [--strict] [--repo-root <path>]
//     scope: comma-separated list of tokens. e.g. "C0,P3,P4,P5"
//     Valid tokens: C0,C1,C2,C3,C4,P3,P4,P5,P6,P7,P8,P9,P10,P11,G1,G2,G3,G4
//   govcheck status [--repo-root <path>]
//   govcheck claims [--namespace <ns>] [--status <status>] [--repo-root <path>]
//
// Exit codes:
//   0 = pass / informational success
//   1 = one or more errors found
//   2 = fatal error
//   3 = usage error (bad arguments)

#include "commands/cmd_validate.h"
#include "commands/cmd_scan.h"
#include "commands/cmd_status.h"
#include "commands/cmd_claims.h"
#include "commands/cmd_deploy_gate.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kUsage =
    "Usage:\n"
    "  govcheck validate --scope <scope> [--strict] [--repo-root <path>]\n"
    "    scope: comma-separated tokens. e.g. C0,P3,P4,P5\n"
    "    Valid: C0-C4, P3-P11, G1-G4\n"
    "  govcheck scan --file <path>\n"
    "  govcheck status [--repo-root <path>]\n"
    "  govcheck claims [--namespace <ns>] [--status <status>] [--repo-root <path>]\n"
    "  govcheck deploy-gate --target <env> [--repo-root <path>]\n";

void print_usage_error(const std::string& message) {
    std::cerr << "govcheck: " << message << "\n" << kUsage;
}

bool find_flag_value(const std::vector<std::string>& args,
                     std::string_view flag,
                     std::string& out_value) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == flag) {
            if (i + 1 >= args.size()) {
                return false;
            }
            out_value = args[i + 1];
            return true;
        }
    }
    return false;
}

bool has_flag(const std::vector<std::string>& args, std::string_view flag) {
    for (const auto& a : args) {
        if (a == flag) return true;
    }
    return false;
}

int run_validate(const std::vector<std::string>& args) {
    std::string scope;
    if (!find_flag_value(args, "--scope", scope) || scope.empty()) {
        print_usage_error("validate: missing required --scope <scope>");
        return 3;
    }

    // ── S0 §4.6 Freeze Exception: Composite --scope Parsing ─────────────
    // EGBOS-RECON-S1-001-REM-02 | Founder Decision: Audit Item 2, Option (a)
    //
    // S0.html §4.6 (line 843) defines scope as single-token:
    //   "Scope Values: C0 (governance pre-check). Future: P3, P4, G1, G2, etc."
    //
    // Master Blueprint §I2 id="slice-S1" (line 6352) requires composite scope
    // for the S1 exit gate:
    //   govcheck validate --scope C0,P3,P4,P5 --strict
    //
    // Master Blueprint is higher authority than S0 spec. Composite comma-
    // separated scope parsing is therefore an authorized, narrow exception
    // to the frozen S0 CLI specification. Exception boundary: composite
    // scope parsing only — no other S0-frozen govcheck behavior is altered.
    // ───────────────────────────────────────────────────────────────────────
    static const std::vector<std::string> valid_tokens = {
        "C0", "C1", "C2", "C3", "C4",
        "P3", "P4", "P5", "P6", "P7", "P8", "P9", "P10", "P11",
        "G1", "G2", "G3", "G4"
    };

    // Parse comma-separated scope string into individual tokens
    std::vector<std::string> scope_tokens;
    {
        std::string token;
        std::istringstream stream(scope);
        while (std::getline(stream, token, ',')) {
            if (!token.empty()) {
                scope_tokens.push_back(token);
            }
        }
    }

    if (scope_tokens.empty()) {
        print_usage_error("validate: --scope contains no valid tokens");
        return 3;
    }

    for (const auto& tok : scope_tokens) {
        bool found = false;
        for (const auto& valid : valid_tokens) {
            if (tok == valid) {
                found = true;
                break;
            }
        }
        if (!found) {
            print_usage_error("validate: invalid scope token '" + tok +
                "'. Valid tokens: C0-C4, P3-P11, G1-G4");
            return 3;
        }
    }

    const bool strict = has_flag(args, "--strict");

    std::string repo_root_str = ".";
    find_flag_value(args, "--repo-root", repo_root_str);
    std::filesystem::path repo_root(repo_root_str);

    return egbos::govcheck::cmd_validate(scope, strict, repo_root);
}

int run_scan(const std::vector<std::string>& args) {
    std::string file_str;
    if (!find_flag_value(args, "--file", file_str) || file_str.empty()) {
        print_usage_error("scan: missing required --file <path>");
        return 3;
    }
    return egbos::govcheck::cmd_scan(std::filesystem::path(file_str));
}

int run_status(const std::vector<std::string>& args) {
    std::string repo_root_str = ".";
    find_flag_value(args, "--repo-root", repo_root_str);
    std::filesystem::path repo_root(repo_root_str);

    return egbos::govcheck::cmd_status(repo_root);
}

int run_claims(const std::vector<std::string>& args) {
    std::string filter_namespace;
    find_flag_value(args, "--namespace", filter_namespace);

    std::string filter_status;
    find_flag_value(args, "--status", filter_status);

    std::string repo_root_str = ".";
    find_flag_value(args, "--repo-root", repo_root_str);
    std::filesystem::path repo_root(repo_root_str);

    return egbos::govcheck::cmd_claims(filter_namespace, filter_status, repo_root);
}

int run_deploy_gate(const std::vector<std::string>& args) {
    std::string target;
    if (!find_flag_value(args, "--target", target) || target.empty()) {
        print_usage_error("deploy-gate: missing required --target <env>");
        return 3;
    }

    std::string repo_root_str = ".";
    find_flag_value(args, "--repo-root", repo_root_str);
    std::filesystem::path repo_root(repo_root_str);

    return egbos::govcheck::cmd_deploy_gate(target, repo_root.string());
}

} // namespace

#ifndef GOVCHECK_TEST_MODE
int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage_error("missing subcommand");
        return 3;
    }

    const std::string subcommand = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);

    if (subcommand == "validate") {
        return run_validate(args);
    }
    if (subcommand == "scan") {
        return run_scan(args);
    }
    if (subcommand == "status") {
        return run_status(args);
    }
    if (subcommand == "claims") {
        return run_claims(args);
    }
    if (subcommand == "deploy-gate") {
        return run_deploy_gate(args);
    }

    print_usage_error("unknown subcommand '" + subcommand + "'");
    return 3;
}
#endif  // GOVCHECK_TEST_MODE
