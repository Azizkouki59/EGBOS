// govcheck/src/graph/cycle_detector.h
// Tarjan's cycle detection (GC-F-001).

#pragma once

#include <string>
#include <vector>

#include "dag_builder.h"
#include "validator/claim_validator.h"

namespace egbos::govcheck {

// Detect cycles in the claim dependency graph using Tarjan's SCC algorithm.
// GC-F-001 (fatal) if any cycle is detected.
// Returns findings with cycle path in message.
std::vector<ValidationFinding> detect_cycles(const ClaimGraph& graph);

}  // namespace egbos::govcheck
