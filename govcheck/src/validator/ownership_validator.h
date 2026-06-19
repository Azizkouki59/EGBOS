// govcheck/src/validator/ownership_validator.h
// team.yaml cross-check (GC-E-019).

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace egbos::govcheck {

// Load active member usernames from governance/team.yaml.
// Returns empty vector on error (caller should treat as fatal).
std::vector<std::string> load_team_members(const std::filesystem::path& team_yaml_path);

}  // namespace egbos::govcheck
