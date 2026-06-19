// govcheck/src/validator/ownership_validator.cpp

#include "ownership_validator.h"

#include <yaml-cpp/yaml.h>

namespace egbos::govcheck {

std::vector<std::string> load_team_members(const std::filesystem::path& team_yaml_path) {
    std::vector<std::string> members;

    YAML::Node root;
    try {
        root = YAML::LoadFile(team_yaml_path.string());
    } catch (const YAML::Exception&) {
        return members;
    }

    if (!root["members"] || !root["members"].IsSequence()) {
        return members;
    }

    for (const auto& member : root["members"]) {
        if (!member["active"] || !member["active"].as<bool>(false)) continue;
        if (!member["username"]) continue;
        members.push_back(member["username"].as<std::string>());
    }

    return members;
}

}  // namespace egbos::govcheck
