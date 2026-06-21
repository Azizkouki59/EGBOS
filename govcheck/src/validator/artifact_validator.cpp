// govcheck/src/validator/artifact_validator.cpp
// SHA-256 proof artifact verification (GC-E-018).
// Spec: S0 Implementation Specification §4.3.2

#include "artifact_validator.h"

#include <openssl/evp.h>

#include <fstream>
#include <iomanip>
#include <sstream>

namespace egbos::govcheck {

// Compute the SHA-256 hex digest of the file at the given path.
// Sets ok=true on success, ok=false on any I/O or digest failure.
static std::string compute_sha256(const std::filesystem::path& path, bool& ok) {
    ok = false;

    std::ifstream file(path, std::ios::binary);
    if (!file) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    char buffer[65536];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx, buffer, static_cast<std::size_t>(file.gcount())) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    ok = true;
    return oss.str();
}

std::vector<ValidationFinding> validate_artifacts(
    const ParsedClaim& claim,
    const std::filesystem::path& repo_root)
{
    std::vector<ValidationFinding> findings;

    for (const auto& artifact : claim.proof_artifacts) {
        auto artifact_path = repo_root / artifact.path;

        if (!std::filesystem::exists(artifact_path)) {
            findings.push_back({
                "GC-E-018",
                Severity::Error,
                claim.claim_id,
                "Proof artifact file not found: " + artifact.path,
                claim.source_file.string(),
                0
            });
            continue;
        }

        bool ok = false;
        std::string computed = compute_sha256(artifact_path, ok);
        if (!ok) {
            findings.push_back({
                "GC-E-018",
                Severity::Error,
                claim.claim_id,
                "Failed to compute SHA-256 for proof artifact: " + artifact.path,
                claim.source_file.string(),
                0
            });
            continue;
        }

        if (computed != artifact.sha256) {
            findings.push_back({
                "GC-E-018",
                Severity::Error,
                claim.claim_id,
                "Proof artifact SHA-256 mismatch for '" + artifact.path +
                    "': declared=" + artifact.sha256 + " computed=" + computed,
                claim.source_file.string(),
                0
            });
        }
    }

    return findings;
}

}  // namespace egbos::govcheck
