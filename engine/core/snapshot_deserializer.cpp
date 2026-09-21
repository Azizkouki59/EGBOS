// engine/core/snapshot_deserializer.cpp
#include "snapshot_deserializer.h"
#include "snapshot_abi.h"
#include "position_engine.h"
#include "risk_enforcer.h"
#include "../l1/lob_engine.h"

#include <openssl/evp.h>
#include <cstring>
#include <iostream>

namespace egbos::core {

bool SnapshotDeserializer::deserialize(
    const std::vector<uint8_t>& data,
    PositionEngine& pos_engine,
    RiskEnforcer& risk_enforcer,
    egbos::l1::LobEngine& lob_engine)
{
    if (data.size() < sizeof(SnapshotHeader)) return false;

    // 1. Verify Hash
    std::vector<uint8_t> mutable_data = data;
    SnapshotHeader* header = reinterpret_cast<SnapshotHeader*>(mutable_data.data());

    uint8_t expected_hash[32];
    std::memcpy(expected_hash, header->hash, 32);
    std::memset(header->hash, 0, 32);

    uint8_t computed_hash[32];
    unsigned int computed_len = 0;
    if (EVP_Digest(mutable_data.data(), mutable_data.size(), computed_hash, &computed_len,
                   EVP_sha256(), nullptr) != 1 || computed_len != 32) {
        return false;
    }

    if (std::memcmp(expected_hash, computed_hash, 32) != 0) return false;

    // Restore hash for safety
    std::memcpy(header->hash, expected_hash, 32);

    // 2. Verify Magic and Versions
    if (header->magic != kSnapshotMagic) return false;
    if (header->format_version != kFormatVersion) return false;
    if (header->schema_version != kSchemaVersion) return false;

    size_t offset = sizeof(SnapshotHeader);

    // 3. Deserialize Positions
    if (offset + sizeof(SerializedPosition) > data.size()) return false;
    SerializedPosition pos;
    std::memcpy(&pos, data.data() + offset, sizeof(pos));
    offset += sizeof(pos);

    PositionEngine::PositionSnapshot pos_snap{};
    pos_snap.position = pos.position;
    pos_snap.order_tag = pos.order_tag;
    pos_snap.x_l = pos.x_l;
    pos_snap.gross_exposure = pos.gross_exposure;
    pos_engine.hydrate_from(pos_snap);

    // 4. Deserialize Risk
    if (offset + sizeof(SerializedRisk) > data.size()) return false;
    SerializedRisk risk;
    std::memcpy(&risk, data.data() + offset, sizeof(risk));
    offset += sizeof(risk);

    RiskEnforcer::RiskSnapshot risk_snap{};
    risk_snap.current_state = risk.current_state;
    risk_enforcer.hydrate_from(risk_snap);

    // 5 & 6. Deserialize LOB Header and Orders via hydration API
    if (offset > data.size()) return false;

    if (!lob_engine.hydrate_from(data.data() + offset, data.size() - offset)) {
        return false;
    }

    return true;
}

} // namespace egbos::core
