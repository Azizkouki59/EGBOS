// govcheck/src/commands/cmd_deploy_gate.h
#pragma once
#include <string>

namespace egbos::govcheck {
int cmd_deploy_gate(const std::string& target, const std::string& repo_root);
}
