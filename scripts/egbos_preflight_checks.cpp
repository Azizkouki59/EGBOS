#include "core/shm_layout.h"
#include "core/wal_writer.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

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

    return 1;
}
