// govcheck/src/reporter/json_reporter.h
// Structured JSON output for all govcheck findings.
// Output schema per S0 Spec §4.5.
// Reuses ValidationFinding and Severity from claim_validator.h.
#pragma once
#include <string>
#include <vector>
#include "validator/claim_validator.h"

namespace egbos::govcheck {

struct ReportSummary {
    int total_claims{0};
    int fatals{0};
    int errors{0};
    int warnings{0};
    bool passed{false};
};

// Build JSON report string from summary and findings.
// Outputs to stdout via the caller; does not write to disk.
std::string build_report(
    const std::string& scope,
    const ReportSummary& summary,
    const std::vector<ValidationFinding>& findings);

} // namespace egbos::govcheck
