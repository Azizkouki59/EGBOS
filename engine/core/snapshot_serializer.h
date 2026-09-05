// engine/core/snapshot_serializer.h
#pragma once

#include <vector>
#include <cstdint>

namespace egbos::core {
class PositionEngine;
class RiskEnforcer;
}

namespace egbos::l1 {
class LobEngine;
}

namespace egbos::core {

class SnapshotSerializer {
public:
    static std::vector<uint8_t> serialize(
        const PositionEngine& pos_engine,
        const RiskEnforcer& risk_enforcer,
        const egbos::l1::LobEngine& lob_engine
    );
};

} // namespace egbos::core
