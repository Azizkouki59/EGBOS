// engine/tests/test_snapshot_contract.cpp
#include <gtest/gtest.h>
#include "../core/snapshot_serializer.h"
#include "../core/snapshot_deserializer.h"
#include "../core/position_engine.h"
#include "../core/risk_enforcer.h"
#include "../core/governance_interface.h"
#include "../core/wal_writer.h"
#include "../core/snapshot_abi.h"
#include "../l1/lob_engine.h"
#include <cstring>

using namespace egbos::core;
using namespace egbos::l1;
#include <openssl/sha.h>   // add near the other includes if not already present

TEST(SnapshotContractTest, SerializationHydrationSucceeds) {
    egbos::wal::WalWriter wal_writer("/tmp/dummy_wal", "/tmp/dummy_wal_shadow");
    GovernanceFlags gov_flags;

    auto pos_engine = std::make_unique<PositionEngine>(wal_writer, gov_flags);
    auto risk_enforcer = std::make_unique<RiskEnforcer>(gov_flags, *pos_engine);
    auto lob_engine = std::make_unique<LobEngine>(100000, 100);

    // Setup some state
    FillEvent f; f.instrument_id = 1; f.quantity = 10; f.signed_qty = 10; f.price = 100000;
    pos_engine->apply_fill(f);

    lob_engine->add_order(1, 100100, 5, true);
    lob_engine->add_order(2, 100200, 10, true);
    lob_engine->add_order(3, 99900, 20, false);

    std::cerr << "Starting serialization\n";
    auto blob = SnapshotSerializer::serialize(*pos_engine, *risk_enforcer, *lob_engine);
    std::cerr << "Serialized, blob size: " << blob.size() << "\n";
    ASSERT_FALSE(blob.empty());

    // Deserialize into fresh engines
    std::cerr << "Creating fresh engines\n";
    auto new_pos = std::make_unique<PositionEngine>(wal_writer, gov_flags);
    auto new_risk = std::make_unique<RiskEnforcer>(gov_flags, *new_pos);
    auto new_lob = std::make_unique<LobEngine>(100000, 100);

    std::cerr << "Hydrating\n";
    bool hydrated = SnapshotDeserializer::deserialize(blob, *new_pos, *new_risk, *new_lob);
    std::cerr << "Hydrated: " << hydrated << "\n";
    ASSERT_TRUE(hydrated);

    // Verify some state via snapshots
    std::cerr << "Verifying state\n";
    PositionEngine::PositionSnapshot snap_pos1, snap_pos2;
    pos_engine->snapshot_into(snap_pos1);
    new_pos->snapshot_into(snap_pos2);
    EXPECT_EQ(snap_pos1.position, snap_pos2.position);

    RiskEnforcer::RiskSnapshot snap_risk1, snap_risk2;
    risk_enforcer->snapshot_into(snap_risk1);
    new_risk->snapshot_into(snap_risk2);
    EXPECT_EQ(snap_risk1.current_state, snap_risk2.current_state);

    std::vector<uint8_t> lob1(sizeof(egbos::l1::LobEngine::LobInternalSnapshot) + 100 * sizeof(egbos::core::SerializedOrder));
    std::vector<uint8_t> lob2(sizeof(egbos::l1::LobEngine::LobInternalSnapshot) + 100 * sizeof(egbos::core::SerializedOrder));
    lob_engine->snapshot_into(lob1.data(), lob1.size());
    new_lob->snapshot_into(lob2.data(), lob2.size());

    std::cerr << "Verifying headers\n";
    egbos::l1::LobEngine::LobInternalSnapshot hdr1, hdr2;
    std::memcpy(&hdr1, lob1.data(), sizeof(hdr1));
    std::memcpy(&hdr2, lob2.data(), sizeof(hdr2));
    EXPECT_EQ(hdr1.anchor_price, hdr2.anchor_price);

    // Verify continuity invariant
    std::cerr << "Continuity check\n";
    auto blob2 = SnapshotSerializer::serialize(*new_pos, *new_risk, *new_lob);
    EXPECT_EQ(blob, blob2);
}

TEST(SnapshotContractTest, DeterministicSerializationIgnoresHeartbeat) {
    egbos::wal::WalWriter wal_writer("/tmp/dummy_wal2", "/tmp/dummy_wal_shadow2");
    GovernanceFlags gov_flags;

    // Risk A: heartbeat = T1
    auto posA = std::make_unique<PositionEngine>(wal_writer, gov_flags);
    auto riskA = std::make_unique<RiskEnforcer>(gov_flags, *posA);
    auto lobA = std::make_unique<LobEngine>(100000, 100);
    riskA->set_heartbeat_tsc_for_test(1000);

    // Risk B: heartbeat = T2 (same logical state)
    auto posB = std::make_unique<PositionEngine>(wal_writer, gov_flags);
    auto riskB = std::make_unique<RiskEnforcer>(gov_flags, *posB);
    auto lobB = std::make_unique<LobEngine>(100000, 100);
    riskB->set_heartbeat_tsc_for_test(9999);

    auto blobA = SnapshotSerializer::serialize(*posA, *riskA, *lobA);
    auto blobB = SnapshotSerializer::serialize(*posB, *riskB, *lobB);

    // serialize(A) == serialize(B) despite different heartbeat TSC
    EXPECT_EQ(blobA, blobB);
}

TEST(SnapshotContractTest, RejectsOldSchemaVersionWithValidHash) {
    egbos::wal::WalWriter wal_writer("/tmp/dummy_wal3", "/tmp/dummy_wal_shadow3");
    GovernanceFlags gov_flags;
    auto pos = std::make_unique<PositionEngine>(wal_writer, gov_flags);
    auto risk = std::make_unique<RiskEnforcer>(gov_flags, *pos);
    auto lob = std::make_unique<LobEngine>(100000, 100);
    lob->add_order(1, 100100, 5, true);

    auto blob = SnapshotSerializer::serialize(*pos, *risk, *lob);

    // Sanity: the untouched blob must deserialize.
    {
        auto pos2 = std::make_unique<PositionEngine>(wal_writer, gov_flags);
        auto risk2 = std::make_unique<RiskEnforcer>(gov_flags, *pos2);
        auto lob2 = std::make_unique<LobEngine>(100000, 100);
        ASSERT_TRUE(SnapshotDeserializer::deserialize(blob, *pos2, *risk2, *lob2));
    }

    // Forge a v1 blob: patch schema_version, then recompute a VALID hash
    // (hash is over the whole blob with the hash field zeroed).
    auto* hdr = reinterpret_cast<SnapshotHeader*>(blob.data());
    hdr->schema_version = 1;
    std::memset(hdr->hash, 0, 32);
    SHA256(blob.data(), blob.size(), hdr->hash);

    auto pos3 = std::make_unique<PositionEngine>(wal_writer, gov_flags);
    auto risk3 = std::make_unique<RiskEnforcer>(gov_flags, *pos3);
    auto lob3 = std::make_unique<LobEngine>(100000, 100);
    EXPECT_FALSE(SnapshotDeserializer::deserialize(blob, *pos3, *risk3, *lob3));
}