// govcheck/src/main.cpp
// govcheck — CLI entry point and command dispatch.
// Spec: S0 Implementation Specification §4.6
//
// Usage:
//   govcheck validate --scope <scope> [--strict] [--repo-root <path>]
//   govcheck status [--repo-root <path>]
//   govcheck claims [--namespace <ns>] [--status <status>] [--repo-root <path>]
//
// Exit codes:
//   0 = pass / informational success
//   1 = one or more errors found
//   2 = fatal error
//   3 = usage error (bad arguments)

#include "commands/cmd_validate.h"
#include "commands/cmd_status.h"
#include "commands/cmd_claims.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kUsage =
    "Usage:\n"
    "  govcheck validate --scope <scope> [--strict] [--repo-root <path>]\n"
    "  govcheck status [--repo-root <path>]\n"
    "  govcheck claims [--namespace <ns>] [--status <status>] [--repo-root <path>]\n";

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

    if (scope != "C0" && scope != "C1" && scope != "C2" && scope != "C3" && scope != "C4") {
        print_usage_error("validate: invalid --scope");
        return 3;
    }

    const bool strict = has_flag(args, "--strict");

    std::string repo_root_str = ".";
    find_flag_value(args, "--repo-root", repo_root_str);
    std::filesystem::path repo_root(repo_root_str);

    return egbos::govcheck::cmd_validate(scope, strict, repo_root);
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
    if (subcommand == "status") {
        return run_status(args);
    }
    if (subcommand == "claims") {
        return run_claims(args);
    }

    print_usage_error("unknown subcommand '" + subcommand + "'");
    return 3;
}
#endif  // GOVCHECK_TEST_MODE
