// engine/core/snapshot_deserializer.h
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

class SnapshotDeserializer {
public:
    static bool deserialize(
        const std::vector<uint8_t>& data,
        PositionEngine& pos_engine,
        RiskEnforcer& risk_enforcer,
        egbos::l1::LobEngine& lob_engine
    );
};

} // namespace egbos::core
