// engine/core/position_engine.h
// EGBOS ENGINE_V3 — Position Engine (L3)
// Specification: EGBOS-SPEC-S3-001

#pragma once

#include <atomic>
#include <cstdint>
#include "wal_writer.h"

namespace egbos::core {

struct FillEvent {
    uint32_t instrument_id;
    int64_t quantity; // Absolute quantity
    int64_t signed_qty; // Positive for buy, negative for sell
    int64_t price;
};

class PositionEngine {
public:
    explicit PositionEngine(wal::WalWriter& wal_writer);
    ~PositionEngine() = default;

    // Disallow copy/move
    PositionEngine(const PositionEngine&) = delete;
    PositionEngine& operator=(const PositionEngine&) = delete;

    void apply_fill(const FillEvent& fill);
    void apply_mass_cancel();
    void sync_position();

private:
    wal::WalWriter& wal_writer_;
    uint64_t next_sequence_{1};

    // Exclusive L3 state
    int64_t position_{0};
    uint64_t order_tag_{0};
    std::atomic<int64_t> x_l_{0};

public:
    // Only CROSS_PLANE_READ accesses this
    std::atomic<int64_t> gross_exposure_{0};

    struct PositionSnapshot {
        int64_t  position;
        uint64_t order_tag;
        int64_t  x_l;
        int64_t  gross_exposure;
    };

    void snapshot_into(PositionSnapshot& out) const noexcept;
    void hydrate_from(const PositionSnapshot& in) noexcept;
};

} // namespace egbos::core
