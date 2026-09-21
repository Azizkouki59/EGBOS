// engine/core/snapshot_serializer.cpp
#include "snapshot_serializer.h"
#include "snapshot_abi.h"
#include "position_engine.h"
#include "risk_enforcer.h"
#include "../l1/lob_engine.h"

#include <openssl/evp.h>
#include <cstring>
#include <algorithm>

namespace egbos::core {

std::vector<uint8_t> SnapshotSerializer::serialize(
    const PositionEngine& pos_engine,
    const RiskEnforcer& risk_enforcer,
    const egbos::l1::LobEngine& lob_engine)
{
    std::vector<uint8_t> blob;

    // 1. Snapshot Header
    SnapshotHeader header{};
    header.magic = kSnapshotMagic;
    header.format_version = kFormatVersion;
    header.schema_version = kSchemaVersion;
    std::memset(header.hash, 0, sizeof(header.hash));

    size_t offset = 0;
    blob.resize(sizeof(header));
    std::memcpy(blob.data() + offset, &header, sizeof(header));
    offset += sizeof(header);

    // 2. Positions
    PositionEngine::PositionSnapshot pos_snap{};
    pos_engine.snapshot_into(pos_snap);

    SerializedPosition pos{};
    pos.position = pos_snap.position;
    pos.order_tag = pos_snap.order_tag;
    pos.x_l = pos_snap.x_l;
    pos.gross_exposure = pos_snap.gross_exposure;

    blob.resize(offset + sizeof(pos));
    std::memcpy(blob.data() + offset, &pos, sizeof(pos));
    offset += sizeof(pos);

    // 3. Risk Limits
    RiskEnforcer::RiskSnapshot risk_snap{};
    risk_enforcer.snapshot_into(risk_snap);

    SerializedRisk risk{};
    risk.current_state = risk_snap.current_state;

    blob.resize(offset + sizeof(risk));
    std::memcpy(blob.data() + offset, &risk, sizeof(risk));
    offset += sizeof(risk);

    // 4 & 5. LOB Engine Top-Level and Orders
    // We allocate a large enough buffer for max possible orders + header
    // In a real system we'd compute exact size, but here we can just resize the vector
    // and then shrink it down to what was actually written.
    size_t lob_max_size = sizeof(egbos::l1::LobEngine::LobInternalSnapshot) +
                          egbos::l1::kMaxOrders * sizeof(SerializedOrder);
    blob.resize(offset + lob_max_size);

    size_t lob_written = lob_engine.snapshot_into(blob.data() + offset, lob_max_size);
    if (lob_written == 0) {
        // Handle serialization failure (buffer too small or logic error)
        // Return empty buffer to signal failure
        return {};
    }

    offset += lob_written;
    blob.resize(offset);

    // 6. Hash computation
    uint8_t hash[32];
    unsigned int hash_len = 0;
    if (EVP_Digest(blob.data(), blob.size(), hash, &hash_len, EVP_sha256(), nullptr) != 1
        || hash_len != 32) {
        return {};
    }

    std::memcpy(blob.data() + offsetof(SnapshotHeader, hash), hash, 32);

    return blob;
}

} // namespace egbos::core
