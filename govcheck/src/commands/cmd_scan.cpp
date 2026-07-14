// govcheck/src/commands/cmd_scan.cpp
// §15.4 S1 source-level governance rule scanner.
// govcheck scan --file <path>
//
// Implements per-file scanning for:
//   GC-E-010  malloc/new/delete/free in engine/ hot-path — S1 warning (S0)
//   GC-WAL-001 State mutation before WAL fsync            — S1 blocking (P1)
//   GC-E-054  Research SIMD/AVX flags non-compliant      — S1 blocking (P2)
//   GC-E-056  SIM_STATE production SHM access            — S1 blocking (C0)
//   GC-E-SECRETS-001  Secret material in tracked file    — S1 blocking (C0)
//
// Also performs a companion claim YAML check for:
//   GC-E-053  V3+ claim amended without accepted RFC     — S1 blocking (C0)
//   (triggered when a violation_GC_E_053.cpp companion file is scanned)
//
// Spec: Master Blueprint §I7.2 GC-E Rule Activation Schedule

#include "commands/cmd_scan.h"
#include "reporter/json_reporter.h"
#include "validator/claim_validator.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace egbos::govcheck {

namespace {

// ── helpers ───────────────────────────────────────────────────────────────────

// Read all lines from a file. Returns empty vector on IO error.
static std::vector<std::string> read_lines(const std::filesystem::path& p) {
    std::vector<std::string> lines;
    std::ifstream in(p);
    if (!in) return lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(std::move(line));
    }
    return lines;
}

// Strip a single-line // comment from a code line.
static std::string strip_cpp_comment(const std::string& line) {
    auto pos = line.find("//");
    return (pos != std::string::npos) ? line.substr(0, pos) : line;
}

// True if the line is a shell or Python comment (starts with #, ignoring ws).
static bool is_hash_comment(const std::string& line) {
    for (char c : line) {
        if (c == ' ' || c == '\t') continue;
        return c == '#';
    }
    return false;
}

// ── GC-E-054 — Research SIMD/AVX flags non-compliant ─────────────────────────
// Blocking from S1 at P2. Spec §I7.2.
// Detects AVX/SSE SIMD intrinsic headers and usage in C/C++ files that are
// not located under research/simd/ and lack a GC-E-054-exempt annotation.

static std::vector<ValidationFinding> scan_gc_e_054(
    const std::filesystem::path& path,
    const std::vector<std::string>& lines)
{
    std::vector<ValidationFinding> findings;

    // Rule only applies to C/C++ source files.
    const auto ext = path.extension().string();
    if (ext != ".cpp" && ext != ".cc" && ext != ".c" &&
        ext != ".h"   && ext != ".hpp") {
        return findings;
    }

    // Files under research/simd/ are the approved SIMD path — exempt.
    std::string path_str = path.generic_string();
    if (path_str.find("research/simd/") != std::string::npos) {
        return findings;
    }

    // An explicit in-file exemption annotation suppresses the finding.
    for (const auto& ln : lines) {
        if (ln.find("GC-E-054-exempt") != std::string::npos) {
            return findings;
        }
    }

    // Regex for SIMD intrinsic headers.
    static const std::regex re_simd_hdr(
        R"(#\s*include\s*[<"](immintrin|xmmintrin|emmintrin|nmmintrin|avxintrin|smmintrin|pmmintrin|tmmintrin|wmmintrin|zmmintrin)\.h[>"])",
        std::regex_constants::ECMAScript);

    // Regex for SIMD types and function prefixes.
    static const std::regex re_simd_usage(
        R"(\b(__m128|__m256|__m512|__mmask|_mm_|_mm256_|_mm512_)\w*)",
        std::regex_constants::ECMAScript);

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const std::string code = strip_cpp_comment(lines[i]);
        if (std::regex_search(code, re_simd_hdr) ||
            std::regex_search(code, re_simd_usage)) {
            findings.push_back({
                "GC-E-054",
                Severity::Error,
                "",
                "Research SIMD/AVX intrinsics detected outside research/simd/ "
                "approved path — requires -ffp-contract=off build gate (§I7.2 GC-E-054)",
                path.string(),
                i + 1
            });
        }
    }
    return findings;
}

// ── GC-E-056 — SIM_STATE production SHM access ───────────────────────────────
// Blocking from S1, immediate at C0. Spec §I7.2.
// Fires when a file contains both a SIM_STATE reference and a /dev/shm/ path,
// indicating a simulation code path is accessing production shared memory
// (simulation isolation boundary violation — AD-013).

static std::vector<ValidationFinding> scan_gc_e_056(
    const std::filesystem::path& path,
    const std::vector<std::string>& lines)
{
    std::vector<ValidationFinding> findings;

    // Cheap pre-check: does the file reference SIM_STATE at all?
    bool has_sim_state = false;
    for (const auto& ln : lines) {
        if (ln.find("SIM_STATE") != std::string::npos) {
            has_sim_state = true;
            break;
        }
    }
    if (!has_sim_state) return findings;

    // Scan every line for /dev/shm/ production path access.
    static const std::regex re_shm(
        R"(/dev/shm/[A-Za-z0-9_\-]+)",
        std::regex_constants::ECMAScript);

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        std::smatch m;
        if (std::regex_search(lines[i], m, re_shm)) {
            findings.push_back({
                "GC-E-056",
                Severity::Error,
                "",
                "SIM_STATE context accesses production /dev/shm/ path '"
                    + m[0].str()
                    + "' — simulation isolation boundary violated (AD-013, §I7.2 GC-E-056)",
                path.string(),
                i + 1
            });
        }
    }
    return findings;
}

// ── GC-E-SECRETS-001 — Secret material in tracked source files ────────────────
// Blocking from S1, immediate at C0. Spec §I7.2.
// Detects hardcoded credentials, API keys, and private key material.
// Enforcement: PROC-008 / Vault HSM (AD-014).

static std::vector<ValidationFinding> scan_gc_e_secrets_001(
    const std::filesystem::path& path,
    const std::vector<std::string>& lines)
{
    std::vector<ValidationFinding> findings;

    // AWS secret access key assignment  (value must be non-trivial, ≥ 8 chars)
    static const std::regex re_aws(
        R"(AWS_SECRET_ACCESS_KEY\s*=\s*["'][^"']{8,}["'])",
        std::regex_constants::ECMAScript);

    // GitHub personal access token (ghp_ prefix, 36+ alphanum chars)
    static const std::regex re_github(
        R"(\bghp_[A-Za-z0-9_]{35,}\b)",
        std::regex_constants::ECMAScript);

    // PEM private key header (RSA, EC, DSA, or generic PRIVATE KEY)
    static const std::regex re_pem(
        R"(-----BEGIN\s+(?:RSA |EC |DSA )?PRIVATE KEY-----)",
        std::regex_constants::ECMAScript);

    // Generic API/auth token assignment (value ≥ 16 chars in quotes)
    static const std::regex re_token(
        R"((?:API_TOKEN|API_KEY|SECRET_KEY|ACCESS_TOKEN|AUTH_TOKEN)\s*=\s*["'][^"']{16,}["'])",
        std::regex_constants::ECMAScript);

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const auto& ln = lines[i];

        // Skip pure comment lines (shell #, C++ //, Python #)
        if (is_hash_comment(ln)) continue;
        const std::string code = strip_cpp_comment(ln);
        if (code.find_first_not_of(" \t\r\n") == std::string::npos) continue;

        std::string desc;
        if (desc.empty() && std::regex_search(ln, re_aws))    desc = "AWS secret access key";
        if (desc.empty() && std::regex_search(ln, re_github)) desc = "GitHub personal access token (ghp_)";
        if (desc.empty() && std::regex_search(ln, re_pem))    desc = "PEM private key material";
        if (desc.empty() && std::regex_search(ln, re_token))  desc = "API token / secret";

        if (!desc.empty()) {
            findings.push_back({
                "GC-E-SECRETS-001",
                Severity::Error,
                "",
                "Secret material detected in tracked file (" + desc
                    + ") — use Vault HSM (AD-014, PROC-008) instead"
                    " (§I7.2 GC-E-SECRETS-001)",
                path.string(),
                i + 1
            });
        }
    }
    return findings;
}

// ── GC-E-053 companion claim YAML check ───────────────────────────────────────
// §15.4 names a violation_GC_E_053.cpp as the scan entry point for a rule
// whose violation lives in a claim YAML, not in C++ source.  When scan is
// called with violation_GC_E_NNN.cpp, this function looks for a companion
// fixture_GC_E_NNN_claim.yaml in the same directory and evaluates it for
// GC-E-053 directly via yaml-cpp (bypassing the parser's filename-ID check).

static std::vector<ValidationFinding> check_companion_claim_yaml(
    const std::filesystem::path& source_file)
{
    std::vector<ValidationFinding> findings;

    // Extract the rule-code portion from the source filename.
    // violation_GC_E_053.cpp  →  rule_part = "GC_E_053"
    const std::string stem   = source_file.stem().string();
    const std::string prefix = "violation_";
    if (stem.size() <= prefix.size() ||
        stem.substr(0, prefix.size()) != prefix) {
        return findings;
    }
    const std::string rule_part = stem.substr(prefix.size());

    // Look for fixture_GC_E_053_claim.yaml beside the source file.
    const auto companion = source_file.parent_path()
                         / ("fixture_" + rule_part + "_claim.yaml");
    if (!std::filesystem::exists(companion)) {
        return findings;
    }

    // Parse the companion YAML directly; do NOT use parse_claim_file() because
    // that enforces claim_id == filename stem (which intentionally differs here).
    YAML::Node root;
    try {
        root = YAML::LoadFile(companion.string());
    } catch (const YAML::Exception&) {
        return findings;  // malformed YAML — not this rule's concern
    }

    // ── GC-E-053: V3+ claim amended without accepted RFC ─────────────────────
    // Extract verification_class, amendments, and rfc_references directly.
    const auto vc_node = root["verification_class"];
    if (!vc_node || vc_node.IsNull()) return findings;
    const std::string vc_str = vc_node.as<std::string>("");

    // Only applies to V3 and V4 claims.
    bool is_v3_plus = (vc_str == "V3" || vc_str == "V4");
    if (!is_v3_plus) return findings;

    // Check amendments sequence (non-empty = at least one amendment entry).
    const auto amend_node = root["amendments"];
    bool has_amendments = amend_node && amend_node.IsSequence()
                          && amend_node.size() > 0;
    if (!has_amendments) return findings;

    // Check rfc_references sequence (must be non-empty to satisfy the rule).
    const auto rfc_node = root["rfc_references"];
    bool has_rfc = rfc_node && rfc_node.IsSequence() && rfc_node.size() > 0;

    const std::string claim_id = root["claim_id"]
                                 ? root["claim_id"].as<std::string>("")
                                 : "";

    if (!has_rfc) {
        findings.push_back({
            "GC-E-053",
            Severity::Error,
            claim_id,
            "V3+ claim has amendments but no rfc_references: an accepted RFC "
            "listing this claim in artifacts_changed is required (§I7.2 GC-E-053)",
            companion.string(),
            0
        });
    }

    return findings;
}

// ── GC-E-010 — malloc/new/delete/free in engine/ hot-path (per-file scan) ────
// Warning from S0. Same patterns as scan_engine_sources in cmd_validate, but
// applied per-file for the govcheck scan subcommand (fixture verification).
// String literals are stripped before matching to avoid false positives.

static std::vector<ValidationFinding> scan_gc_e_010(
    const std::filesystem::path& path,
    const std::vector<std::string>& lines)
{
    std::vector<ValidationFinding> findings;

    const auto ext = path.extension().string();
    if (ext != ".cpp" && ext != ".cc" && ext != ".c" &&
        ext != ".h"   && ext != ".hpp") {
        return findings;
    }

    static const std::regex re_malloc(R"(\bmalloc\s*\()",   std::regex_constants::ECMAScript);
    static const std::regex re_free  (R"(\bfree\s*\()",     std::regex_constants::ECMAScript);
    static const std::regex re_new   (R"(\bnew\s+(\w|\[))", std::regex_constants::ECMAScript);
    static const std::regex re_delete(R"(\bdelete\s+(\[|\w))", std::regex_constants::ECMAScript);

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        // Strip C++ comment
        std::string code = strip_cpp_comment(lines[i]);

        // Strip string literals
        {
            std::string stripped;
            stripped.reserve(code.size());
            bool in_str = false;
            for (std::size_t j = 0; j < code.size(); ++j) {
                char c = code[j];
                if (!in_str && c == '"') {
                    in_str = true; stripped += ' ';
                } else if (in_str) {
                    if (c == '\\') ++j;
                    else if (c == '"') in_str = false;
                    stripped += ' ';
                } else {
                    stripped += c;
                }
            }
            code = std::move(stripped);
        }

        if (std::regex_search(code, re_malloc) ||
            std::regex_search(code, re_free)   ||
            std::regex_search(code, re_new)    ||
            std::regex_search(code, re_delete)) {
            findings.push_back({
                "GC-E-010",
                Severity::Warning,
                "",
                "Prohibited memory allocation/deallocation in engine/ hot path",
                path.string(),
                i + 1
            });
        }
    }
    return findings;
}

// ── GC-WAL-001 — State mutation before WAL fsync ──────────────────────────────
// Blocking from S1 at P1. Spec §I7.2, AD-012, §4.6 WAL-before-state protocol.
//
// The three-phase commit requires:
//   Phase 1: Prepare
//   Phase 2: WAL write_sync() — blocks until O_DSYNC
//   Phase 3: Apply — mutate state ONLY after Phase 2 returns
//
// Violation: atomic state mutation appearing BEFORE a WAL write call in the
// same source function/block. Detection heuristic:
//   - File contains an atomic store/fetch_add/exchange on a non-WAL variable
//   - AND a WAL write call (wal_write_pending, write_sync, WalWrite) appears
//     AFTER the state mutation in the file
//   - The state mutation line number is less than the WAL write line number

static std::vector<ValidationFinding> scan_gc_wal_001(
    const std::filesystem::path& path,
    const std::vector<std::string>& lines)
{
    std::vector<ValidationFinding> findings;

    const auto ext = path.extension().string();
    if (ext != ".cpp" && ext != ".cc" && ext != ".c" &&
        ext != ".h"   && ext != ".hpp") {
        return findings;
    }

    // Patterns for state mutation (atomic operations on state variables)
    static const std::regex re_state_mutation(
        R"(\.(?:store|fetch_add|fetch_sub|fetch_or|fetch_and|exchange|compare_exchange_weak|compare_exchange_strong)\s*\()",
        std::regex_constants::ECMAScript);

    // Patterns for WAL write calls (call expression, not forward declaration).
    // We exclude lines that start with a type keyword (forward declarations
    // look like: `void wal_write_pending();`).
    // A call site looks like: `    wal_write_pending();` or `write_sync(...)`
    static const std::regex re_wal_write(
        R"(\b(?:wal_write_pending|write_sync|WalWrite|wal_write)\s*\()",
        std::regex_constants::ECMAScript);
    // Declaration indicator: return type at the start of the stripped line
    static const std::regex re_forward_decl(
        R"(^\s*(?:void|int|bool|auto|static|inline|explicit|virtual)\s+\w)",
        std::regex_constants::ECMAScript);

    // Pass 1: find first mutation line
    int first_mutation_line = -1;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const std::string code = strip_cpp_comment(lines[i]);
        if (std::regex_search(code, re_state_mutation)) {
            first_mutation_line = i + 1;
            break;
        }
    }
    if (first_mutation_line < 0) return findings;

    // Pass 2: find first WAL write CALL (not declaration) AFTER the mutation
    int first_wal_line = -1;
    for (int i = first_mutation_line; i < static_cast<int>(lines.size()); ++i) {
        const std::string code = strip_cpp_comment(lines[i]);
        if (std::regex_search(code, re_wal_write) &&
            !std::regex_search(code, re_forward_decl)) {
            first_wal_line = i + 1;
            break;
        }
    }

    // Violation: state mutation exists AND WAL write call appears after it
    if (first_wal_line > first_mutation_line) {
        findings.push_back({
            "GC-WAL-001",
            Severity::Error,
            "",
            "State mutation before WAL fsync: atomic operation at line "
                + std::to_string(first_mutation_line)
                + " precedes WAL write at line "
                + std::to_string(first_wal_line)
                + " — violates WAL-before-state three-phase commit (AD-012, §4.6 GC-WAL-001)",
            path.string(),
            first_mutation_line
        });
    }
    return findings;
}



}  // namespace

// ── cmd_scan ──────────────────────────────────────────────────────────────────

int cmd_scan(const std::filesystem::path& file_path)
{
    if (!std::filesystem::exists(file_path) ||
        !std::filesystem::is_regular_file(file_path)) {
        std::cerr << "govcheck scan: file not found: " << file_path << "\n";
        return 3;
    }

    const auto lines = read_lines(file_path);
    std::vector<ValidationFinding> all_findings;

    // Source-level rule scans
    for (auto& f : scan_gc_e_010        (file_path, lines)) all_findings.push_back(std::move(f));
    for (auto& f : scan_gc_wal_001      (file_path, lines)) all_findings.push_back(std::move(f));
    for (auto& f : scan_gc_e_054        (file_path, lines)) all_findings.push_back(std::move(f));
    for (auto& f : scan_gc_e_056        (file_path, lines)) all_findings.push_back(std::move(f));
    for (auto& f : scan_gc_e_secrets_001(file_path, lines)) all_findings.push_back(std::move(f));

    // Companion claim YAML check (GC-E-053, activated via violation_GC_E_053.cpp)
    for (auto& f : check_companion_claim_yaml(file_path)) all_findings.push_back(std::move(f));

    // Build report summary
    ReportSummary summary;
    for (const auto& f : all_findings) {
        if      (f.severity == Severity::Fatal)   ++summary.fatals;
        else if (f.severity == Severity::Error)   ++summary.errors;
        else if (f.severity == Severity::Warning) ++summary.warnings;
    }
    summary.passed = (summary.fatals == 0 && summary.errors == 0);

    std::cout << build_report("scan:" + file_path.string(), summary, all_findings) << "\n";

    if (summary.fatals > 0) return 2;
    if (summary.errors > 0) return 1;
    return 0;
}

}  // namespace egbos::govcheck
