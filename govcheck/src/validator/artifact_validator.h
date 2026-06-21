// govcheck/src/validator/artifact_validator.h
// SHA-256 proof artifact verification (GC-E-018).
// Spec: S0 Implementation Specification §4.3.2

#pragma once

#include <filesystem>
#include <vector>

#include "parser/yaml_parser.h"
#include "validator/claim_validator.h"

namespace egbos::govcheck {

// For each proof_artifact entry in the claim, compute the SHA-256 of the file
// at the declared path (resolved relative to repo_root) and compare it to the
// declared sha256 field.
//
// Emits GC-E-018 (error) for:
//   - SHA-256 mismatch between declared and computed hash.
//   - File not found at the declared path.
std::vector<ValidationFinding> validate_artifacts(
    const ParsedClaim& claim,
    const std::filesystem::path& repo_root);

}  // namespace egbos::govcheck
