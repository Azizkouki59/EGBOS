// engine/tests/test_gap_framework.cpp
// EGBOS S5-REQ-002 — Offline Deterministic Feed-Gap Detection Framework
//
// Specification refs  : S5-REQ-002 → CLAIM-L0-002
// Governed component  : L0 Feed Ingestor (engine/l0/ingestor.cpp)
// Classification      : LOCAL_SYNTHETIC  (never LIVE_OPERATIONAL)
// Source revision     : 2663389
//
// PURPOSE:
//   Provide a deterministic, offline test framework for validating the gap-detection
//   and staleness-emission logic in FeedIngestor.
//
// CRITICAL LIMITATION — READ BEFORE USING THIS FRAMEWORK:
//   All tests in this file use SYNTHETIC, LOCALLY GENERATED sequence streams.
//   Gaps are injected programmatically, not observed from a physical exchange feed.
//
//   THEREFORE:
//     - Gap detection logic is VALIDATED (code is correct)
//     - Gap detection is NOT PROVEN against a live exchange feed
//   Stale detection uses last_recv_time_ns_, now initialized to 0 (no-packet-received
//   sentinel) and updated with a CLOCK_MONOTONIC_RAW timestamp injected by the CQE
//   caller in live_feed_listener.cpp. In these tests, the timestamp is injected
//   synthetically per test to maintain determinism.
//
//   V4 promotion of CLAIM-L0-002 requires LIVE_OPERATIONAL evidence:
//     - Physical exchange feed connection (not UDP loopback)
//     - Real exchange sequence numbers observed
//     - Hardware-clock timestamps (not mock values)
//     - PCAP capture for sequence provenance
//

//
// SCOPE:
//   This file tests only via FeedIngestor's public API.
//   engine/l0/ingestor.cpp is NOT modified here.

#include <gtest/gtest.h>
#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"

#include <cstring>
#include <cstdint>

#if defined(_WIN32)
#  include <io.h>
#  define OPEN_FUNC  _open
#  define CLOSE_FUNC _close
#  define DEV_NULL   "NUL"
#  define O_FLAGS    (_O_WRONLY | _O_APPEND)
#else
#  include <unistd.h>
#  include <fcntl.h>
#  define OPEN_FUNC  open
#  define CLOSE_FUNC close
#  define DEV_NULL   "/dev/null"
#  define O_FLAGS    (O_WRONLY | O_APPEND)
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────

// Build a minimal 16-byte payload: [8-byte seq_num | 8-byte zeros]
static void make_payload(uint8_t* buf, uint64_t seq) {
    std::memset(buf, 0, 16);
    std::memcpy(buf, &seq, sizeof(seq));
}

// ── Fixture ───────────────────────────────────────────────────────────────────
class GapFrameworkTest : public ::testing::Test {
protected:
    egbos::shm::MarketEventsRegion market_events_;
    egbos::shm::GovernanceFlags    gov_flags_{};
    int                            tier0_fd_{-1};

    void SetUp() override {
        tier0_fd_ = OPEN_FUNC(DEV_NULL, O_FLAGS);
        ASSERT_GE(tier0_fd_, 0) << "Cannot open tier0 sink";
    }

    void TearDown() override {
        if (tier0_fd_ >= 0) CLOSE_FUNC(tier0_fd_);
    }

    egbos::l0::FeedIngestor make_ingestor() {
        return egbos::l0::FeedIngestor(&market_events_, &gov_flags_, tier0_fd_);
    }
};

// ── Test A: Deterministic gap in controlled sequence ─────────────────────────
//
// Input sequence : 1, 2, 3, 5  (FeedIngestor initializes expected_seq_=1)
// Expected gap   : 4 is missing
// Expected events:
//   slot(0) = normalized payload for seq 1
//   slot(1) = normalized payload for seq 2
//   slot(2) = normalized payload for seq 3
//   slot(3) = GapEvent (synthetic marker 0xFF, type=1)  ← gap before 5
//   slot(4) = normalized payload for seq 5
//
// NOTE: Sequences are 1-based to match FeedIngestor constructor (expected_seq_=1).
// Using 1001-based sequences would trigger a spurious gap (1 → 1001).
// The gap detection logic is identical; only the numeric range changes.
//
// Classification: LOCAL_SYNTHETIC
// This test does NOT prove physical exchange-feed gap detection.
TEST_F(GapFrameworkTest, DetectsGap_1004_Missing) {
    auto ingestor = make_ingestor();
    uint8_t payload[16];

    // Feed seq 1, 2, 3 — no gap
    for (uint64_t seq : {1ULL, 2ULL, 3ULL}) {
        make_payload(payload, seq);
        ingestor.process_payload(payload, sizeof(payload), 12345);
    }

    // Feed seq 5 — gap: 4 is missing
    make_payload(payload, 5ULL);
    ingestor.process_payload(payload, sizeof(payload), 12345);

    egbos::shm::MarketEventSlot slot;

    // Drain: seq 1001, 1002, 1003 → 3 normal slots
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(market_events_.pop(slot))
            << "Expected normal slot " << (i + 1) << " missing";
        // Normal slots: byte[0] != 0xFF
        EXPECT_NE(slot.data[0], 0xFF)
            << "Slot " << i << " incorrectly classified as synthetic";
    }

    // GapEvent must be the NEXT slot (emitted before seq 1005)
    ASSERT_TRUE(market_events_.pop(slot))
        << "GapEvent not emitted after sequence gap (1004 missing)";
    EXPECT_EQ(slot.data[0], 0xFF) << "GapEvent: expected synthetic marker 0xFF";
    EXPECT_EQ(slot.data[1], 1)    << "GapEvent: expected event_type=1 (GAP)";

    // seq 1005 normalized slot follows
    ASSERT_TRUE(market_events_.pop(slot))
        << "Normalized slot for seq 1005 not emitted";
    EXPECT_NE(slot.data[0], 0xFF)
        << "Slot for seq 1005 incorrectly classified as synthetic";

    // No further events
    EXPECT_FALSE(market_events_.pop(slot))
        << "Unexpected extra event in ring";
}

// ── Test B: Gap size = 1 (adjacent skip) ─────────────────────────────────────
TEST_F(GapFrameworkTest, DetectsGap_SingleMissing) {
    auto ingestor = make_ingestor();
    uint8_t payload[16];

    make_payload(payload, 1ULL);
    ingestor.process_payload(payload, sizeof(payload), 12345);

    // seq 3 — skips 2
    make_payload(payload, 3ULL);
    ingestor.process_payload(payload, sizeof(payload), 12345);

    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events_.pop(slot)); // seq 1
    ASSERT_TRUE(market_events_.pop(slot)); // GapEvent
    EXPECT_EQ(slot.data[0], 0xFF);
    EXPECT_EQ(slot.data[1], 1);
    ASSERT_TRUE(market_events_.pop(slot)); // seq 3
    EXPECT_FALSE(market_events_.pop(slot));
}

// ── Test C: Gap size > 1 (multi-packet loss) ─────────────────────────────────
// Uses seq [1,6]: skips 2,3,4,5 (4 packets lost).
// Starting at 1 to match expected_seq_=1 in FeedIngestor constructor.
TEST_F(GapFrameworkTest, DetectsGap_MultiMissing) {
    auto ingestor = make_ingestor();
    uint8_t payload[16];

    make_payload(payload, 1ULL);
    ingestor.process_payload(payload, sizeof(payload), 12345);

    // seq 6 — skips 2,3,4,5 (4 packets lost)
    make_payload(payload, 6ULL);
    ingestor.process_payload(payload, sizeof(payload), 12345);

    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events_.pop(slot)); // seq 1
    ASSERT_TRUE(market_events_.pop(slot)); // GapEvent
    EXPECT_EQ(slot.data[0], 0xFF);
    EXPECT_EQ(slot.data[1], 1);
    ASSERT_TRUE(market_events_.pop(slot)); // seq 6
    EXPECT_FALSE(market_events_.pop(slot));
}

// ── Test D: No gap in ordered sequence ───────────────────────────────────────
TEST_F(GapFrameworkTest, NoGap_OrderedSequence) {
    auto ingestor = make_ingestor();
    uint8_t payload[16];

    for (uint64_t seq = 1; seq <= 10; ++seq) {
        make_payload(payload, seq);
        ingestor.process_payload(payload, sizeof(payload), 12345);
    }

    egbos::shm::MarketEventSlot slot;
    int normal_count = 0;
    while (market_events_.pop(slot)) {
        EXPECT_NE(slot.data[0], 0xFF)
            << "Unexpected synthetic event in ordered sequence at slot " << normal_count;
        ++normal_count;
    }
    EXPECT_EQ(normal_count, 10) << "Expected exactly 10 normal slots";
}

// ── Test F: Duplicate and Out-of-order sequences ─────────────────────────────
// Input: 1, 2, 2 (duplicate), 1 (out of order).
// Existing contract (ingestor.cpp line 78): Drop late/duplicate packets.
TEST_F(GapFrameworkTest, DropsDuplicateAndOutOfOrder) {
    auto ingestor = make_ingestor();
    uint8_t payload[16];

    // seq 1, 2
    for (uint64_t seq : {1ULL, 2ULL}) {
        make_payload(payload, seq);
        ingestor.process_payload(payload, sizeof(payload), 12345);
    }

    // duplicate seq 2
    make_payload(payload, 2ULL);
    ingestor.process_payload(payload, sizeof(payload), 12345);

    // out-of-order seq 1
    make_payload(payload, 1ULL);
    ingestor.process_payload(payload, sizeof(payload), 12345);

    egbos::shm::MarketEventSlot slot;

    // We should only receive 2 normalized slots (1 and 2), no synthetic gaps.
    ASSERT_TRUE(market_events_.pop(slot));
    EXPECT_NE(slot.data[0], 0xFF);

    ASSERT_TRUE(market_events_.pop(slot));
    EXPECT_NE(slot.data[0], 0xFF);

    EXPECT_FALSE(market_events_.pop(slot))
        << "Duplicate/out-of-order packets were not dropped";
}

// ── Test E: Deterministic replay invariant ────────────────────────────────────
// Feed the same sequence twice; the resulting event streams must be identical.
// This validates the determinism property required by S5-REQ-004's replay proof.
TEST_F(GapFrameworkTest, DeterministicReplay_SameInputSameOutput) {
    uint8_t payload[16];
    const uint64_t sequences[] = {1001, 1002, 1003, 1005};

    // Run 1
    {
        // Static storage: MarketEventsRegion is 8 MB; stack overflow if local.
        static egbos::shm::MarketEventsRegion ring1;
        static egbos::shm::GovernanceFlags    flags1{};
        int fd1 = OPEN_FUNC(DEV_NULL, O_FLAGS);
        ASSERT_GE(fd1, 0);
        egbos::l0::FeedIngestor ingestor1(&ring1, &flags1, fd1);
        for (auto seq : sequences) {
            make_payload(payload, seq);
            ingestor1.process_payload(payload, sizeof(payload), 12345);
        }

        // Drain run 1
        egbos::shm::MarketEventSlot s;
        std::vector<std::array<uint8_t, sizeof(s.data)>> run1_events;
        while (ring1.pop(s)) {
            std::array<uint8_t, sizeof(s.data)> arr;
            std::memcpy(arr.data(), s.data, sizeof(s.data));
            run1_events.push_back(arr);
        }
        CLOSE_FUNC(fd1);

        // Run 2
        static egbos::shm::MarketEventsRegion ring2;
        static egbos::shm::GovernanceFlags    flags2{};
        int fd2 = OPEN_FUNC(DEV_NULL, O_FLAGS);
        ASSERT_GE(fd2, 0);
        egbos::l0::FeedIngestor ingestor2(&ring2, &flags2, fd2);
        for (auto seq : sequences) {
            make_payload(payload, seq);
            ingestor2.process_payload(payload, sizeof(payload), 12345);
        }

        egbos::shm::MarketEventSlot s2;
        std::vector<std::array<uint8_t, sizeof(s2.data)>> run2_events;
        while (ring2.pop(s2)) {
            std::array<uint8_t, sizeof(s2.data)> arr;
            std::memcpy(arr.data(), s2.data, sizeof(s2.data));
            run2_events.push_back(arr);
        }
        CLOSE_FUNC(fd2);

        ASSERT_EQ(run1_events.size(), run2_events.size())
            << "Replay: event count diverges between runs";
        for (size_t i = 0; i < run1_events.size(); ++i) {
            EXPECT_EQ(run1_events[i], run2_events[i])
                << "Replay: event " << i << " is not byte-identical";
        }
    }
}

// ── S5-REQ-002: Clock / Timestamp Contract Tests ──────────────────────────────
// Classification: LOCAL_SYNTHETIC
// Verifies that last_recv_time_ns_ is driven by the injected rx_timestamp_ns
// (CLOCK_MONOTONIC_RAW domain, D4) and not by a mock value.

TEST_F(GapFrameworkTest, ClockContract_FirstPacketInitializesTimestamp) {
    // Before any packet: last_recv_time_ns_ starts at 0 (no-packet sentinel).
    // check_staleness(0) must be a no-op (guard on last_recv_time_ns_ == 0).
    auto ingestor = make_ingestor();
    ingestor.check_staleness(0ULL);
    egbos::shm::MarketEventSlot slot;
    EXPECT_FALSE(market_events_.pop(slot))
        << "No stale event before first packet";

    // Send first packet with a synthetic D4 timestamp.
    constexpr uint64_t kFirstTs = 100'000'000ULL; // 100ms boot-relative
    uint8_t payload[24] = {0};
    uint64_t seq = 1;
    std::memcpy(payload, &seq, sizeof(uint64_t));
    ingestor.process_payload(payload, sizeof(payload), kFirstTs);

    // Consume the normal event.
    ASSERT_TRUE(market_events_.pop(slot));
    // Staleness check at exact receipt time must not emit stale event.
    ingestor.check_staleness(kFirstTs);
    EXPECT_FALSE(market_events_.pop(slot))
        << "No stale event at exact packet timestamp";
}

TEST_F(GapFrameworkTest, ClockContract_TimestampIsNonZeroAfterFirstPacket) {
    // After a packet with ts > 0, staleness check beyond the threshold
    // must emit STALE — proving last_recv_time_ns_ != 0 (mock is gone).
    auto ingestor = make_ingestor();
    constexpr uint64_t kFirstTs = 1'000'000'000ULL; // 1s
    constexpr uint64_t kNowTs   = kFirstTs + 600'000'000ULL; // 1.6s > 500ms

    uint8_t payload[24] = {0};
    uint64_t seq = 1;
    std::memcpy(payload, &seq, sizeof(uint64_t));
    ingestor.process_payload(payload, sizeof(payload), kFirstTs);

    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events_.pop(slot)); // consume normal event

    ingestor.check_staleness(kNowTs);
    ASSERT_TRUE(market_events_.pop(slot))
        << "Stale event expected: proves last_recv_time_ns_ was set non-zero";
    EXPECT_EQ(slot.data[0], 0xFF); // synthetic marker
    EXPECT_EQ(slot.data[1], 2);    // STALE type
}

TEST_F(GapFrameworkTest, ClockContract_MonotonicTimestampsNoSpuriousStale) {
    // Successive packets with monotonically increasing timestamps must not
    // trigger stale detection between packets.
    auto ingestor = make_ingestor();
    uint8_t payload[24] = {0};
    constexpr uint64_t kBase = 500'000'000ULL;
    egbos::shm::MarketEventSlot slot;

    for (uint64_t i = 0; i < 5; ++i) {
        uint64_t seq = i + 1;
        std::memcpy(payload, &seq, sizeof(uint64_t));
        uint64_t ts = kBase + i * 10'000'000ULL; // +10ms each
        ingestor.process_payload(payload, sizeof(payload), ts);
        ASSERT_TRUE(market_events_.pop(slot)); // consume normal event
        // Check staleness at exact receive time: must NOT be stale.
        ingestor.check_staleness(ts);
        EXPECT_FALSE(market_events_.pop(slot))
            << "No stale event at packet i=" << i;
    }
}


// V4 promotion of CLAIM-L0-002 requires an artifact with the following fields.
// This comment block defines the mandatory schema for a future LIVE_OPERATIONAL
// proof artifact. It must NOT be generated by this test file.
//
// Required artifact fields for LIVE_OPERATIONAL classification:
// {
//   "artifact_id": "S5-REQ-002-GAP-DETECTION-LIVE-v1",
//   "requirement_id": "S5-REQ-002",
//   "classification": "LIVE_OPERATIONAL",          // NOT LOCAL_SYNTHETIC
//   "governed_component": "CLAIM-L0-002",
//   "feed_source": "<exchange_id>",                // real exchange identifier
//   "feed_protocol": "<udp_multicast|fix|...>",
//   "observation_start_utc": "<ISO-8601>",
//   "observation_end_utc": "<ISO-8601>",
//   "pcap_sha256": "<sha256 of captured PCAP>",    // provenance
//   "sequence_numbers_observed": <N>,
//   "gaps_detected": <M>,
//   "gap_detection_accuracy_pct": 100.0,           // must be 100.0
//   "stale_threshold_ns": 500000000,
//   "stale_events_detected": <K>,
//   "hardware_clock": "CLOCK_MONOTONIC (rdtsc cross-validated)",
//   "source_revision": "<git sha>",
//   "v4_satisfied": true                           // set only if all criteria pass
// }
//
// This artifact does NOT exist. CLAIM-L0-002 remains at V3.
