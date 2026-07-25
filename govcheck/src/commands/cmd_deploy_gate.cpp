// govcheck/src/commands/cmd_deploy_gate.cpp
#include "cmd_deploy_gate.h"
#include <iostream>
#include "cmd_validate.h"

namespace egbos::govcheck {

int cmd_deploy_gate(const std::string& target, const std::string& repo_root) {
    if (target != "staging" && target != "production") {
        std::cerr << "govcheck: deploy-gate: invalid target '" << target << "'\n";
        return 3;
    }

    std::cout << "[deploy-gate] Validating requirements for target " << target << "...\n";

    // Run validate with C0,P3,P7,G1 scope as required by staging
    int result = 0;
    if (target == "staging") {
        result = cmd_validate("C0,P3,P7,G1", true, repo_root);
    } else {
        result = cmd_validate("C0,G2", true, repo_root);
    }
    if (result != 0) {
        std::cerr << "[deploy-gate] Validation failed. Deployment blocked.\n";
        return result;
    }

    // Additional hardcoded checks could go here
    std::cout << "[deploy-gate] All requirements met. Gate green.\n";
    return 0;
}

} // namespace egbos::govcheck
