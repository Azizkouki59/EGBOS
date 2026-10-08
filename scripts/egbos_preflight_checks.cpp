#include "core/shm_layout.h"
#include "core/wal_writer.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/evp.h>

using namespace egbos::wal;

namespace {

struct PositionReconstruction {
    std::array<int64_t, egbos::core::kMaxInstruments> positions{};
    uint64_t fill_applied_records{0};
    bool valid{true};
};

void consume_valid_record(const WalRecord& record, void* context) noexcept {
    auto* state = static_cast<PositionReconstruction*>(context);

    if (record.record_type !=
        static_cast<uint32_t>(WalRecordType::FILL_APPLIED)) {
        return;
    }

    if (record.instrument_id >= egbos::core::kMaxInstruments) {
        state->valid = false;
        return;
    }

    const std::size_t index =
        static_cast<std::size_t>(record.instrument_id);
    const int64_t delta = record.quantity_delta;
    const int64_t current = state->positions[index];

    const bool overflow =
        (delta > 0 &&
         current > (std::numeric_limits<int64_t>::max() - delta)) ||
        (delta < 0 &&
         current < (std::numeric_limits<int64_t>::min() - delta));

    if (overflow) {
        state->valid = false;
        return;
    }

    state->positions[index] = current + delta;
    ++state->fill_applied_records;
}

template <typename RegionT>
const RegionT* map_existing_shm_readonly(const char* path) noexcept {
    const int fd = ::shm_open(path, O_RDONLY, 0);
    if (fd < 0) {
        return nullptr;
    }
    void* const addr = ::mmap(
        nullptr, sizeof(RegionT), PROT_READ, MAP_SHARED, fd, 0);
    ::close(fd);
    if (addr == MAP_FAILED) {
        return nullptr;
    }
    return static_cast<const RegionT*>(addr);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [wal-scan|position-reconstruct]" << std::endl;
        return 1;
    }
    std::string cmd = argv[1];

    if (cmd == "wal-scan") {
        if (argc < 3) {
            std::cerr << "Missing WAL path" << std::endl;
            return 1;
        }
        std::string wal_path = argv[2];
        if (!std::filesystem::exists(wal_path)) {
            std::cout << "PASS (No WAL found)" << std::endl;
            return 0;
        }
        auto result = WalWriter::scan_wal(wal_path.c_str());
        if (!result.was_clean) {
            std::cout << "FAIL (WAL not clean. valid_records=" << result.valid_records << ")" << std::endl;
            return 1;
        }
        std::cout << "PASS (WAL clean. valid_records=" << result.valid_records << ")" << std::endl;
        return 0;
    }

    if (cmd == "position-reconstruct") {
        if (argc < 3) {
            std::cerr << "Missing WAL path" << std::endl;
            return 1;
        }

        std::string wal_path = argv[2];
        if (!std::filesystem::exists(wal_path)) {
            std::cout << "NOT APPLICABLE (No WAL found; no preflight position state to reconstruct)"
                      << std::endl;
            return 0;
        }

        PositionReconstruction reconstruction{};

        const auto result = WalWriter::scan_wal(
            wal_path.c_str(),
            consume_valid_record,
            &reconstruction);

        if (!result.was_clean) {
            std::cout << "FAIL (WAL integrity/sequence validation failed)"
                      << std::endl;
            return 1;
        }

        if (!reconstruction.valid) {
            std::cout
                << "FAIL (WAL-only position reconstruction invalid: "
                   "instrument_id out of range or signed quantity overflow)"
                << std::endl;
            return 1;
        }

        std::cout
            << "PASS (WAL-only position reconstruction; "
               "FILL_APPLIED records="
            << reconstruction.fill_applied_records << ")" << std::endl;

        bool any_nonzero = false;
        for (std::size_t instrument_id = 0;
             instrument_id < reconstruction.positions.size();
             ++instrument_id) {
            const int64_t position =
                reconstruction.positions[instrument_id];
            if (position != 0) {
                any_nonzero = true;
                std::cout << "  instrument_id=" << instrument_id
                          << " reconstructed_position=" << position
                          << std::endl;
            }
        }

        if (!any_nonzero) {
            std::cout << "  all reconstructed instrument positions are zero"
                      << std::endl;
        }

        std::cout
            << "  NOTE: this is WAL-only reconstruction, not the live-engine "
               "INV-001 comparison."
            << std::endl;

        return 0;
    }

    if (cmd == "risk-latches") {
        const auto* region =
            map_existing_shm_readonly<egbos::shm::RiskLimitsRegion>(
                egbos::shm::kRiskLimitsPath);
        if (region == nullptr) {
            std::cout
                << "NOT APPLICABLE (no live process holding "
                << egbos::shm::kRiskLimitsPath << ")" << std::endl;
            return 0;
        }
        const int64_t pos_limit =
            region->pos_limit.load(std::memory_order_acquire);
        const bool kill_switch =
            region->kill_switch.load(std::memory_order_acquire);
        if (kill_switch) {
            std::cout << "FAIL (kill_switch latch is set: pos_limit="
                      << pos_limit << ")" << std::endl;
            return 1;
        }
        std::cout << "PASS (kill_switch=false, pos_limit=" << pos_limit
                  << ")" << std::endl;
        return 0;
    }

    if (cmd == "governance-flags") {
        const auto* region =
            map_existing_shm_readonly<egbos::shm::GovernanceFlags>(
                egbos::shm::kGovernanceFlagsPath);
        if (region == nullptr) {
            std::cout
                << "NOT APPLICABLE (no live process holding "
                << egbos::shm::kGovernanceFlagsPath << ")" << std::endl;
            return 0;
        }
        const bool feed_stale =
            region->feed_stale.load(std::memory_order_acquire);
        const bool halt_required =
            region->halt_required.load(std::memory_order_acquire);
        if (halt_required) {
            std::cout << "FAIL (halt_required is set; feed_stale="
                      << (feed_stale ? "true" : "false") << ")" << std::endl;
            return 1;
        }
        std::cout << "PASS (halt_required=false, feed_stale="
                  << (feed_stale ? "true" : "false") << ")" << std::endl;
        return 0;
    }

    if (cmd == "wal-recovery-record") {
        if (argc < 3) {
            std::cerr << "Missing WAL path" << std::endl;
            return 1;
        }
        std::string wal_path = argv[2];
        if (!std::filesystem::exists(wal_path)) {
            std::cerr << "FAIL (WAL file not found: " << wal_path << ")" << std::endl;
            return 1;
        }

        PositionReconstruction reconstruction{};
        const auto result = WalWriter::scan_wal(
            wal_path.c_str(), consume_valid_record, &reconstruction);

        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digest_len = 0;
        const bool hash_ok = EVP_Digest(
            reconstruction.positions.data(),
            reconstruction.positions.size() * sizeof(int64_t),
            digest, &digest_len, EVP_sha256(), nullptr) == 1;
        if (!hash_ok) {
            std::cerr << "FAIL (SHA-256 digest of position state failed)" << std::endl;
            return 1;
        }
        std::ostringstream hash_hex;
        hash_hex << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < digest_len; ++i) {
            hash_hex << std::setw(2) << static_cast<unsigned int>(digest[i]);
        }

        const auto now = std::chrono::system_clock::now();
        const std::time_t now_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream date_str;
        date_str << std::put_time(std::gmtime(&now_t), "%Y-%m-%d");
        std::ostringstream out_path;
        out_path << "governance/recovery/RECOVERY-RECORD-" << date_str.str() << ".yaml";

        std::filesystem::create_directories("governance/recovery");
        std::ofstream out(out_path.str());
        if (!out.is_open()) {
            std::cerr << "FAIL (cannot write " << out_path.str() << ")" << std::endl;
            return 1;
        }

        out << "# REC-008 recovery record - WAL-only portion (steps 1-3)" << "\n";
        out << "# Generated by scripts/egbos_preflight_checks.cpp wal-recovery-record" << "\n";
        out << "# Spec reference: EGBOS_MASTER.md line 1748, REC-008 (line 3239)" << "\n";
        out << "#" << "\n";
        out << "# Fields marked PENDING require the Founder and, where live" << "\n";
        out << "# capital was deployed, RFP reconciliation against real exchange" << "\n";
        out << "# data. This tool does not and cannot fill them." << "\n";
        out << "schema_version: 1" << "\n";
        out << "document_id: \"RECOVERY-RECORD-" << date_str.str() << "\"" << "\n";
        out << "wal_source_path: \"" << wal_path << "\"" << "\n";
        out << "wal_clean: " << (result.was_clean ? "true" : "false") << "\n";
        out << "wal_truncation_point: " << result.truncation_byte_offset << "\n";
        out << "last_valid_sequence: " << result.last_valid_sequence << "\n";
        out << "records_replayed: " << result.valid_records << "\n";
        out << "position_reconstruction_valid: " << (reconstruction.valid ? "true" : "false") << "\n";
        out << "position_state_hash: \"" << hash_hex.str() << "\"" << "\n";
        out << "rfp_match_status: \"PENDING\"" << "\n";
        out << "signature:" << "\n";
        out << "  signer: \"PENDING\"" << "\n";
        out << "  date: \"PENDING\"" << "\n";
        out << "  git_commit_sha: \"PENDING\"" << "\n";
        out.close();

        std::cout << "RECOVERY-RECORD written: " << out_path.str() << std::endl;
        std::cout << "wal_truncation_point=" << result.truncation_byte_offset
                  << " records_replayed=" << result.valid_records
                  << " position_state_hash=" << hash_hex.str() << std::endl;
        std::cout << "NOTE: rfp_match_status and signature are PENDING. "
                     "This record has no governance force until the Founder "
                     "completes REC-008 steps 4-7 and signs." << std::endl;

        if (!result.was_clean) {
            std::cout << "FAIL (WAL was not clean; truncation point identifies "
                         "the corruption boundary per REC-008 step 1-2)" << std::endl;
            return 1;
        }
        return 0;
    }

    if (cmd == "internal-position-reconstruct") {
        // REC-010 step 3: WAL-internal position reconstruction,
        // independent of exchange connectivity. Emits
        // internal_positions_{date}.yaml per instrument_id.
        // Does NOT perform RFP reconciliation (step 5) or any
        // exchange call - that requires live connectivity and
        // is out of scope for this tool.
        if (argc < 3) {
            std::cerr << "Missing WAL path" << std::endl;
            return 1;
        }
        std::string wal_path = argv[2];
        std::string out_override;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--output" && i + 1 < argc) {
                out_override = argv[i + 1];
                ++i;
            }
        }
        if (!std::filesystem::exists(wal_path)) {
            std::cerr << "FAIL (WAL file not found: " << wal_path << ")" << std::endl;
            return 1;
        }

        PositionReconstruction reconstruction{};
        const auto result = WalWriter::scan_wal(
            wal_path.c_str(), consume_valid_record, &reconstruction);

        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digest_len = 0;
        const bool hash_ok = EVP_Digest(
            reconstruction.positions.data(),
            reconstruction.positions.size() * sizeof(int64_t),
            digest, &digest_len, EVP_sha256(), nullptr) == 1;
        if (!hash_ok) {
            std::cerr << "FAIL (SHA-256 digest of position state failed)" << std::endl;
            return 1;
        }
        std::ostringstream hash_hex;
        hash_hex << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < digest_len; ++i) {
            hash_hex << std::setw(2) << static_cast<unsigned int>(digest[i]);
        }

        const auto now = std::chrono::system_clock::now();
        const std::time_t now_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream date_str;
        date_str << std::put_time(std::gmtime(&now_t), "%Y-%m-%d");

        std::string out_path = out_override;
        if (out_path.empty()) {
            std::ostringstream default_path;
            default_path << "internal_positions_" << date_str.str() << ".yaml";
            out_path = default_path.str();
        }

        std::ofstream out(out_path);
        if (!out.is_open()) {
            std::cerr << "FAIL (cannot write " << out_path << ")" << std::endl;
            return 1;
        }

        out << "# REC-010 step 3: WAL-internal position reconstruction" << "\n";
        out << "# Independent of exchange connectivity per spec." << "\n";
        out << "# Generated by scripts/wal_replay_reconcile.sh" << "\n";
        out << "# NOTE: this is NOT an RFP-reconciled record. Step 5" << "\n";
        out << "# (compare against exchange RFP response) has not run." << "\n";
        out << "schema_version: 1" << "\n";
        out << "wal_source_path: \"" << wal_path << "\"" << "\n";
        out << "wal_clean: " << (result.was_clean ? "true" : "false") << "\n";
        out << "records_replayed: " << result.valid_records << "\n";
        out << "position_reconstruction_valid: " << (reconstruction.valid ? "true" : "false") << "\n";
        out << "internal_position_state_hash: \"" << hash_hex.str() << "\"" << "\n";
        out << "positions:" << "\n";
        bool any_nonzero = false;
        for (std::size_t i = 0; i < reconstruction.positions.size(); ++i) {
            const int64_t position = reconstruction.positions[i];
            if (position != 0) {
                any_nonzero = true;
                out << "  - instrument_id: " << i << "\n";
                out << "    position: " << position << "\n";
            }
        }
        if (!any_nonzero) {
            out << "  []" << "\n";
        }
        out << "rfp_reconciled: false" << "\n";
        out << "rfp_delta: \"PENDING\"" << "\n";
        out.close();

        std::cout << "internal_positions written: " << out_path << std::endl;
        std::cout << "internal_position_state_hash=" << hash_hex.str()
                  << " records_replayed=" << result.valid_records << std::endl;
        std::cout << "NOTE: rfp_reconciled=false. Step 5 (RFP comparison) "
                     "requires live exchange connectivity and has not run."
                  << std::endl;

        if (!result.was_clean || !reconstruction.valid) {
            std::cout << "FAIL (WAL not clean or reconstruction invalid; see above)" << std::endl;
            return 1;
        }
        return 0;
    }

    if (cmd == "stratum0-precheck") {
        // REC-009 step 1 only: confirm WAL Primary and WAL Shadow
        // are both unreadable via filesystem check. Steps 2-10
        // require mounting a physical Stratum-0 device on a
        // separate SATA controller (spec: /dev/sata0), which this
        // tool cannot do and does not attempt to simulate.
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0]
                      << " stratum0-precheck <primary_path> <shadow_path>"
                      << std::endl;
            return 1;
        }
        std::string primary_path = argv[2];
        std::string shadow_path = argv[3];

        bool primary_readable = false;
        bool shadow_readable = false;

        if (std::filesystem::exists(primary_path)) {
            const auto r = WalWriter::scan_wal(primary_path.c_str());
            primary_readable = r.was_clean;
        }
        if (std::filesystem::exists(shadow_path)) {
            const auto r = WalWriter::scan_wal(shadow_path.c_str());
            shadow_readable = r.was_clean;
        }

        std::cout << "primary_path=" << primary_path
                  << " readable=" << (primary_readable ? "true" : "false")
                  << std::endl;
        std::cout << "shadow_path=" << shadow_path
                  << " readable=" << (shadow_readable ? "true" : "false")
                  << std::endl;

        if (!primary_readable && !shadow_readable) {
            std::cout << "CONFIRMED: both WAL Primary and WAL Shadow are "
                         "unreadable/corrupt. REC-009 controller-failure "
                         "condition (step 1) is met." << std::endl;
            std::cout << "NEXT: REC-009 steps 2-10 require mounting the "
                         "physical Stratum-0 device on a separate SATA "
                         "controller. This machine has no such device; "
                         "those steps cannot be automated here." << std::endl;
            return 0;
        }

        std::cout << "NOT APPLICABLE (REC-009 controller-failure condition "
                     "not met: at least one of primary/shadow is readable; "
                     "ordinary REC-001/REC-008 recovery applies instead)"
                  << std::endl;
        return 0;
    }

    return 1;
}
