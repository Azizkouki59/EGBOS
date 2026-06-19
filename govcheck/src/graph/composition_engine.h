// govcheck/src/graph/composition_engine.h
// Composed class calculation (GC-E-017, GC-E-024).

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "dag_builder.h"
#include "parser/yaml_parser.h"
#include "validator/claim_validator.h"

namespace egbos::govcheck {

// Compute effective composed verification class for each claim.
// Rule: composed_class = min(own_class, min(all transitive dep classes))
// Returns findings for any mismatch with declared composed_verification_class.
std::vector<ValidationFinding> verify_composed_classes(
    const std::vector<ParsedClaim>& claims,
    const ClaimGraph& graph);

}  // namespace egbos::govcheck
