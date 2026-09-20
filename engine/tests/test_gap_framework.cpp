// engine/tests/test_gap_framework.cpp
// EGBOS S5-REQ-002 — Offline Deterministic Feed-Gap Detection Framework
//
// Specification refs  : S5-REQ-002 → CLAIM-L0-002
// Governed component  : L0 Feed Ingestor (engine/l0/ingestor.cpp)
// Classification      : LOCAL_SYNTHETIC  (never LIVE_OPERATIONAL)
// Source revision     : 2663389
//
// S2-REQ-002 STATUS: NOT IMPLEMENTED (2026-09-12)
//   Gap detection requires extracting U/u update IDs from Binance JSON.
//   The previous 8-byte binary sequence-prefix stub (Finding 7, 2026-09-12 audit)
//   was removed. process_payload() now forwards all payloads unconditionally.
//   Tests that previously asserted gap-event emission are rewritten to assert
//   passthrough behavior explicitly. They will break on purpose when real
//   JSON-aware gap detection is added, forcing a deliberate update.
//   To re-implement: zero-allocation JSON parser for U/u extraction (ADR required).
//   CLAIM-L0-002 remains at V3.
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

// Helper: build a minimal payload starting with '{' (0x7b) so it looks like
// the real Binance JSON wire format that process_payload() now receives.
// The remaining bytes encode an identifying marker for test differentiation.
static size_t make_payload(uint8_t* buf, uint64_t seq) {
    char json_str[128];
    int len = snprintf(
        json_str,
        sizeof(json_str),
        "{\"e\":\"depthUpdate\",\"U\":%llu,\"u\":%llu,"
        "\"b\":[[\"1.0\",\"1.0\"]],\"a\":[]}",
        (unsigned long long)seq,
        (unsigned long long)seq);
    memcpy(buf, json_str, len);
    return len;
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

// S2-REQ-002 (gap detection) is NOT implemented. process_payload() forwards
// all payloads unconditionally — no sequence validation, no gap events.
// Tests A, B, C, F below assert this passthrough behavior explicitly.
// They will break on purpose when real JSON gap detection is re-implemented,
// forcing a deliberate update. See: Option-A patch 2026-09-12.

// ── Test A: Passthrough with sequence gap input ───────────────────────────────
//
// Old behavior: seq 1,2,3,5 → 3 normal slots + GapEvent + 1 normal = 5 events
// New behavior: all 4 payloads forwarded → 4 normal slots, no gap event.
//
// Classification: LOCAL_SYNTHETIC
TEST_F(GapFrameworkTest, DetectsGap_1004_Missing_S2REQ002NotYetImplemented) {
    auto ingestor = make_ingestor();
    uint8_t payload[128]{};

    // Feed seq 1, 2, 3, 5 (4 would be the gap under old logic)
    for (uint64_t seq : {1ULL, 2ULL, 3ULL, 5ULL}) {
        size_t len = make_payload(payload, seq);
        ingestor.process_payload(payload, len, 12345);
    }

    egbos::shm::MarketEventSlot slot;

    // All 4 payloads forwarded, no gap event emitted.
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(market_events_.pop(slot))
            << "Expected normal slot " << (i + 1) << " missing — passthrough broken";
        EXPECT_NE(slot.data[0], 0xFF)
            << "Slot " << i << " is synthetic (0xFF) — gap detection fired unexpectedly (S2-REQ-002 DEFERRED)";
    }

    // Ring must be empty — no synthetic gap event.
    EXPECT_FALSE(market_events_.pop(slot))
        << "Extra event in ring: gap detection should be a no-op (S2-REQ-002 DEFERRED)";
}

// ── Test B: Adjacent-skip passthrough ────────────────────────────────────────
TEST_F(GapFrameworkTest, DetectsGap_SingleMissing_S2REQ002NotYetImplemented) {
    auto ingestor = make_ingestor();
    uint8_t payload[128]{};

    size_t len = make_payload(payload, 1ULL);
    ingestor.process_payload(payload, len, 12345);

    len = make_payload(payload, 3ULL); // seq 2 would have been a gap
    ingestor.process_payload(payload, len, 12345);

    egbos::shm::MarketEventSlot slot;
    // Both forwarded, no gap event.
    ASSERT_TRUE(market_events_.pop(slot));
    EXPECT_NE(slot.data[0], 0xFF);
    ASSERT_TRUE(market_events_.pop(slot));
    EXPECT_NE(slot.data[0], 0xFF);
    EXPECT_FALSE(market_events_.pop(slot))
        << "Unexpected extra event (S2-REQ-002 DEFERRED)";
}

// ── Test C: Multi-packet-loss passthrough ─────────────────────────────────────
TEST_F(GapFrameworkTest, DetectsGap_MultiMissing_S2REQ002NotYetImplemented) {
    auto ingestor = make_ingestor();
    uint8_t payload[128]{};

    size_t len = make_payload(payload, 1ULL);
    ingestor.process_payload(payload, len, 12345);

    len = make_payload(payload, 6ULL); // skips 2,3,4,5
    ingestor.process_payload(payload, len, 12345);

    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events_.pop(slot));
    EXPECT_NE(slot.data[0], 0xFF);
    ASSERT_TRUE(market_events_.pop(slot));
    EXPECT_NE(slot.data[0], 0xFF);
    EXPECT_FALSE(market_events_.pop(slot))
        << "Unexpected extra event (S2-REQ-002 DEFERRED)";
}

// ── Test D: No gap in ordered sequence ───────────────────────────────────────
TEST_F(GapFrameworkTest, NoGap_OrderedSequence) {
    auto ingestor = make_ingestor();
    uint8_t payload[128]{};

    for (uint64_t seq = 1; seq <= 10; ++seq) {
        size_t len = make_payload(payload, seq);
        ingestor.process_payload(payload, len, 12345);
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

// ── Test F: All payloads forwarded regardless of sequence ─────────────────────
// Old contract: seq 1,2 forwarded; duplicate seq 2 and out-of-order seq 1 dropped.
// New contract: ALL non-empty payloads forwarded unconditionally (S2-REQ-002 DEFERRED).
// This test will break when real gap detection is implemented.
TEST_F(GapFrameworkTest, DropsDuplicateAndOutOfOrder_S2REQ002NotYetImplemented) {
    auto ingestor = make_ingestor();
    uint8_t payload[128]{};

    // seq 1, 2, duplicate seq 2, out-of-order seq 1 — all 4 forwarded under passthrough
    for (uint64_t seq : {1ULL, 2ULL, 2ULL, 1ULL}) {
        size_t len = make_payload(payload, seq);
        ingestor.process_payload(payload, len, 12345);
    }

    egbos::shm::MarketEventSlot slot;

    // All 4 forwarded — no filtering, no synthetic events.
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(market_events_.pop(slot))
            << "Slot " << i << " missing — passthrough dropped a payload unexpectedly";
        EXPECT_NE(slot.data[0], 0xFF)
            << "Slot " << i << " is synthetic — gap detection fired (S2-REQ-002 DEFERRED)";
    }
    EXPECT_FALSE(market_events_.pop(slot))
        << "Extra event in ring beyond the 4 forwarded payloads";
}

// ── Test E: Deterministic replay invariant ────────────────────────────────────
// Feed the same sequence twice; the resulting event streams must be identical.
// This validates the determinism property required by S5-REQ-004's replay proof.
TEST_F(GapFrameworkTest, DeterministicReplay_SameInputSameOutput) {
    uint8_t payload[128]{};
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
            size_t len = make_payload(payload, seq);
            ingestor1.process_payload(payload, len, 12345);
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
            size_t len = make_payload(payload, seq);
            ingestor2.process_payload(payload, len, 12345);
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
    uint8_t payload[128]{};
    size_t len = make_payload(payload, 1ULL);
    ingestor.process_payload(payload, len, kFirstTs);

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
    // Pure clock/staleness contract test: disable Tier-0 durability.
    egbos::l0::FeedIngestor ingestor(
        &market_events_, &gov_flags_, -1);
    constexpr uint64_t kFirstTs = 1'000'000'000ULL; // 1s
    constexpr uint64_t kNowTs   = kFirstTs + 600'000'000ULL; // 1.6s > 500ms

    uint8_t payload[128]{};
    size_t len = make_payload(payload, 1ULL);
    ingestor.process_payload(payload, len, kFirstTs);

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
    // Pure clock/staleness contract test: disable Tier-0 durability.
    egbos::l0::FeedIngestor ingestor(
        &market_events_, &gov_flags_, -1);
    uint8_t payload[128]{};
    constexpr uint64_t kBase = 500'000'000ULL;
    egbos::shm::MarketEventSlot slot;

    for (uint64_t i = 0; i < 5; ++i) {
        uint64_t seq = i + 1;
        size_t len = make_payload(payload, seq);
        uint64_t ts = kBase + i * 10'000'000ULL; // +10ms each
        ingestor.process_payload(payload, len, ts);
        ASSERT_TRUE(market_events_.pop(slot)); // consume normal event
        // Check staleness at exact receive time: must NOT be stale.
        ingestor.check_staleness(ts);
        EXPECT_FALSE(market_events_.pop(slot))
            << "No stale event at packet i=" << i;
    }
}

// ── Test: run_loop ordering contract — stale event before process_payload ─────
//
// Models the exact bug fixed in BinanceFeedListener::run_loop() (2026-09-20):
//
//   T0: frame received → process_payload(T0) → last_recv_time_ns_ = T0
//   T1 = T0 + kStaleThresholdNs + 1 ns
//   [run_loop is blocked in SSL_read for >500ms; no check_staleness is called]
//   Frame arrives at T1.
//
// CORRECT ordering (post-fix):
//   check_staleness(T1)  →  idle = T1 - T0 > 500ms → STALE event emitted
//   process_payload(T1)  →  last_recv_time_ns_ = T1 → state = FEED_NORMAL
//
// WRONG ordering (pre-fix):
//   process_payload(T1)  →  last_recv_time_ns_ = T1 → state = FEED_NORMAL
//   check_staleness(T1)  →  idle = T1 - T1 = 0     → no stale event (bug)
//
// Classification: LOCAL_SYNTHETIC
// Specification : REG-FEED-STALE-001 / S2-REQ-004

// ── Test: Exact Threshold (499,999,999 ns vs 500,000,001 ns) ───────────────
TEST_F(GapFrameworkTest, StaleThresholdExactBoundaries) {
    egbos::l0::FeedIngestor ingestor(&market_events_, &gov_flags_, -1);

    constexpr uint64_t T0 = 1'000'000'000ULL;
    uint8_t payload[128]{};
    egbos::shm::MarketEventSlot slot;

    // First frame at T0
    size_t len = make_payload(payload, 1ULL);
    ingestor.process_payload(payload, len, T0);
    ASSERT_TRUE(market_events_.pop(slot)); // Consume normal event

    // Check at exactly 499,999,999 ns (under 500ms threshold)
    constexpr uint64_t T_UNDER = T0 + 499'999'999ULL;
    ingestor.check_staleness(T_UNDER);
    EXPECT_FALSE(market_events_.pop(slot)) << "Must NOT emit stale at 499,999,999 ns";

    // Check at exactly 500,000,000 ns (exactly on boundary, must NOT be stale per > check)
    constexpr uint64_t T_EXACT = T0 + 500'000'000ULL;
    ingestor.check_staleness(T_EXACT);
    EXPECT_FALSE(market_events_.pop(slot)) << "Must NOT emit stale at exact 500,000,000 ns";

    // Check at exactly 500,000,001 ns (over 500ms threshold)
    constexpr uint64_t T_OVER = T0 + 500'000'001ULL;
    ingestor.check_staleness(T_OVER);
    ASSERT_TRUE(market_events_.pop(slot)) << "Must emit stale at 500,000,001 ns";
    EXPECT_EQ(slot.data[0], 0xFF);
    EXPECT_EQ(slot.data[1], 2); // FEED_STALE
}

TEST_F(GapFrameworkTest, StaleDetected_WhenCheckStalenessPrecedesProcessPayload) {
    // Tier-0 durability disabled (-1 fd) so the test is not influenced by
    // fsync timing; this matches the staleness-only test pattern used by
    // ClockContract_TimestampIsNonZeroAfterFirstPacket above.
    egbos::l0::FeedIngestor ingestor(&market_events_, &gov_flags_, -1);

    constexpr uint64_t T0 = 1'000'000'000ULL;          // 1 s
    constexpr uint64_t T1 = T0 + 500'000'000ULL + 1ULL; // > 500 ms later

    uint8_t payload[128]{};

    // Step 1: first frame arrives at T0.
    size_t len = make_payload(payload, 1ULL);
    ingestor.process_payload(payload, len, T0);

    // Consume the normal market event.
    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events_.pop(slot)) << "Expected normal event after T0 frame";
    EXPECT_NE(slot.data[0], 0xFF) << "T0 frame must not be synthetic";

    // Step 2 (the fix): evaluate staleness BEFORE processing the T1 frame.
    // This mirrors the fixed run_loop() ordering:
    //   check_staleness(T1) — sees idle = T1 - T0 > 500ms → STALE
    ingestor.check_staleness(T1);

    // Exactly one STALE synthetic event must be observable.
    ASSERT_TRUE(market_events_.pop(slot))
        << "STALE synthetic event expected: gap T0->T1 > 500ms was not detected";
    EXPECT_EQ(slot.data[0], 0xFF) << "Synthetic marker must be 0xFF";
    EXPECT_EQ(slot.data[1], 2)    << "Event type must be STALE (2)";

    // No second stale event from the same check_staleness call.
    EXPECT_FALSE(market_events_.pop(slot))
        << "Duplicate stale event: check_staleness must emit at most one event";

    // Step 3: the T1 frame arrives (process_payload overwrites last_recv_time_ns_
    // and transitions state back to FEED_NORMAL).
    len = make_payload(payload, 2ULL);
    ingestor.process_payload(payload, len, T1);

    // Consume the normal T1 market event.
    ASSERT_TRUE(market_events_.pop(slot)) << "Expected normal event after T1 frame";
    EXPECT_NE(slot.data[0], 0xFF) << "T1 frame must not be synthetic";

    // Step 4: verify no duplicate stale event is emitted for the same gap
    // when the next staleness check runs at T1 (ingestor is now FEED_NORMAL).
    ingestor.check_staleness(T1);
    EXPECT_FALSE(market_events_.pop(slot))
        << "Duplicate stale event on second check_staleness(T1): gap already resolved";
}

// ── Test: wrong ordering would miss the stale event (documents the pre-fix bug) ─
//
// If process_payload is called BEFORE check_staleness (the pre-fix order),
// the stale gap between T0 and T1 is invisible.  This test asserts that the
// correct (post-fix) ordering is REQUIRED to observe the event.
//
// The test drives FeedIngestor directly with the wrong ordering and verifies
// that no STALE event is produced — confirming that the listener ordering fix
// (check_staleness before process_payload) is the necessary and sufficient
// condition.
//
// Classification: LOCAL_SYNTHETIC
TEST_F(GapFrameworkTest, NoStaleDetected_WhenProcessPayloadPrecedesCheckStaleness) {
    egbos::l0::FeedIngestor ingestor(&market_events_, &gov_flags_, -1);

    constexpr uint64_t T0 = 1'000'000'000ULL;
    constexpr uint64_t T1 = T0 + 500'000'000ULL + 1ULL;

    uint8_t payload[128]{};

    // T0 frame.
    size_t len = make_payload(payload, 1ULL);
    ingestor.process_payload(payload, len, T0);
    egbos::shm::MarketEventSlot slot;
    ASSERT_TRUE(market_events_.pop(slot));

    // WRONG order (pre-fix): process_payload first, then check_staleness.
    len = make_payload(payload, 2ULL);
    ingestor.process_payload(payload, len, T1); // overwrites last_recv_time_ns_
    ASSERT_TRUE(market_events_.pop(slot));       // consume normal T1 event

    ingestor.check_staleness(T1); // idle = T1 - T1 = 0 → no stale event

    EXPECT_FALSE(market_events_.pop(slot))
        << "Pre-fix ordering hides the stale gap: no STALE event is emitted "
           "(this is the bug that the run_loop fix resolves)";
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
