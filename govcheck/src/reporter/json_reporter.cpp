// govcheck/src/reporter/json_reporter.cpp
// Authority: EGBOS_MASTER_augmented.html §B1.3.3
// Spec: S0 Implementation Specification §4.5
#include "reporter/json_reporter.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace egbos::govcheck {

static std::string utc_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string build_report(
    const std::string& scope,
    const ReportSummary& summary,
    const std::vector<ValidationFinding>& findings)
{
    nlohmann::json report;
    report["govcheck_version"] = "1.0.0";
    report["scope"]            = scope;
    report["timestamp"]        = utc_timestamp();
    report["summary"] = {
        {"total_claims", summary.total_claims},
        {"fatals",       summary.fatals},
        {"errors",       summary.errors},
        {"warnings",     summary.warnings},
        {"passed",       summary.passed}
    };

    report["findings"] = nlohmann::json::array();
    for (const auto& f : findings) {
        report["findings"].push_back({
            {"code",     f.code},
            {"severity", to_string(f.severity)},
            {"claim_id", f.claim_id},
            {"message",  f.message},
            {"file",     f.file},
            {"line",     f.line}
        });
    }

    return report.dump(2);
}

} // namespace egbos::govcheck
