// engine/core/position_engine.cpp
// EGBOS ENGINE_V3 — Position Engine (L3)

#include "position_engine.h"
#include <cmath>

#if defined(__linux__)
#include <time.h>
#endif

namespace egbos::core {

static uint64_t clock_monotonic_raw() {
#if defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
#else
    return 0; // Mock for non-linux builds
#endif
}

PositionEngine::PositionEngine(wal::WalWriter& wal_writer, GovernanceFlags& gov_flags)
    : wal_writer_(wal_writer), governance_flags_(gov_flags) {}

void PositionEngine::apply_fill(const FillEvent& fill) {
    // Step 1: Write WAL record FIRST — before any state change
    wal::WalRecord record;
    record.record_type = static_cast<uint32_t>(wal::WalRecordType::FILL_APPLIED);
    record.sequence    = next_sequence_++;
    record.instrument_id = fill.instrument_id;
    record.order_id = 0;
    record.quantity_delta = fill.signed_qty;
    record.price       = fill.price;
    record.payload_length = 0;
    record.tsc_before_write = clock_monotonic_raw();
    wal_writer_.write_sync(&record);
    record.tsc_after_write = clock_monotonic_raw();

    // Step 2: State mutation — only AFTER WAL is fsync-confirmed
    position_ += fill.signed_qty;
    gross_exposure_.fetch_add(std::abs(fill.signed_qty),
                              std::memory_order_release);
}

void PositionEngine::apply_mass_cancel() {
    wal::WalRecord record;
    record.record_type = static_cast<uint32_t>(wal::WalRecordType::MASS_CANCEL);
    record.sequence    = next_sequence_++;
    record.instrument_id = 0;
    record.order_id = 0;
    record.quantity_delta = 0;
    record.price = 0;
    record.payload_length = 0;
    record.tsc_before_write = clock_monotonic_raw();
    wal_writer_.write_sync(&record);
    record.tsc_after_write = clock_monotonic_raw();

    order_tag_ = 0; // Clear all in-flight orders
    // Update gross exposure to reflect position only
    gross_exposure_.store(std::abs(position_), std::memory_order_release);
}

void PositionEngine::snapshot_into(PositionSnapshot& out) const noexcept {
    out.position = position_;
    out.order_tag = order_tag_;
    out.x_l = x_l_.load(std::memory_order_acquire);
    out.gross_exposure = gross_exposure_.load(std::memory_order_acquire);
}

void PositionEngine::hydrate_from(const PositionSnapshot& in) noexcept {
    position_ = in.position;
    order_tag_ = in.order_tag;
    x_l_.store(in.x_l, std::memory_order_release);
    gross_exposure_.store(in.gross_exposure, std::memory_order_release);
}

void PositionEngine::sync_position() {
    wal::WalRecord record;
    record.record_type = static_cast<uint32_t>(wal::WalRecordType::POSITION_SYNC);
    record.sequence    = next_sequence_++;
    record.instrument_id = 0;
    record.order_id = 0;
    record.quantity_delta = position_;
    record.price = 0;
    record.payload_length = 0;
    record.tsc_before_write = clock_monotonic_raw();
    wal_writer_.write_sync(&record);
    record.tsc_after_write = clock_monotonic_raw();
}

} // namespace egbos::core
