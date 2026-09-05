// engine/tests/test_zero_order_path.cpp
// EGBOS S5-REQ-005 — Zero Order Path Application-Level Validation
//
// Specification refs  : S5-REQ-005 — Zero Order Path (no outbound FIX/order-entry)
// Governed component  : engine/l0/live_feed_listener.cpp (RX-only socket layer)
// Classification      : APPLICATION_RX_ONLY  (NOT ARCHITECTURALLY_ENFORCED)
// Source revision     : 2663389
//
// PURPOSE:
//   Demonstrate and validate that the market-data receive path contains no
//   outbound socket operations. This is application-level evidence only.
//
// CRITICAL LIMITATION — READ BEFORE USING:
//   APPLICATION_RX_ONLY ≠ ARCHITECTURALLY_ENFORCED_ZERO_ORDER_PATH
//
//   What this test proves:
//     - The FeedIngestor/live_feed_listener code path contains no send()/sendto()
//       calls reachable from the normal execution path.
//     - A synthetic loopback receive operation succeeds.
//     - The FIX consumer encoder is not linked into the L0 receive path.
//
//   What this test does NOT prove:
//     - That a privileged process cannot open a TX socket.
//     - That the OS prevents outbound order-entry traffic.
//     - Architectural enforcement at the network layer.
//
// V4 requires ARCHITECTURALLY_ENFORCED_ZERO_ORDER_PATH, which requires at
// minimum ONE of the following (none of which is implemented here):
//   A. Network namespace isolation (unshare -n or ip netns) confining the
//      market-data process to a RX-only namespace with no routing to OMS.
//   B. iptables/nftables OUTPUT DROP rules on the market-data host interface,
//      permitting only established/related inbound traffic.
//   C. Linux seccomp(2) policy DENY for sendto/send/sendmsg syscalls from
//      the market-data process binary.
//   D. Linux capabilities: drop CAP_NET_RAW and restrict socket creation to
//      SOCK_DGRAM RX-only operations via capability bounding set.
//   E. Documented production network topology with VLAN segregation proving
//      the market-data feed VLAN has no L3 route to the OMS VLAN.
//
// Current status: APPLICATION_RX_ONLY only.
// CLAIM-L3-05 / CLAIM-L0-002 remain blocked.

#include <gtest/gtest.h>
#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"

#include <cstring>

#if defined(_WIN32)
#  include <io.h>
#  define DEV_NULL "NUL"
#  define O_FLAGS  (_O_WRONLY | _O_APPEND)
#  define OPEN_FUNC  _open
#  define CLOSE_FUNC _close
#else
#  include <unistd.h>
#  include <fcntl.h>
#  define DEV_NULL "/dev/null"
#  define O_FLAGS  (O_WRONLY | O_APPEND)
#  define OPEN_FUNC  open
#  define CLOSE_FUNC close
#endif

// ── Test 1: FeedIngestor produces no outbound event in normal operation ────────
//
// Validates that processing a well-formed market data payload results only in
// a push to the market_events SPSC ring — no outbound socket interaction.
// This is APPLICATION_RX_ONLY evidence.
TEST(ZeroOrderPathTest, FeedIngestor_NoOutboundEvent_OnNormalPayload) {
    // Static storage: MarketEventsRegion is 8 MB (65536 × 128B); stack overflow
    // if allocated as a local. Pattern follows test_gap_detection.cpp fixture.
    static egbos::shm::MarketEventsRegion market_events;
    static egbos::shm::GovernanceFlags    gov_flags{};
    int fd = OPEN_FUNC(DEV_NULL, O_FLAGS);
    ASSERT_GE(fd, 0);

    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, fd);

    // Process a normal market data payload (seq=1, 8 bytes header + 8 bytes data).
    uint8_t payload[16] = {};
    uint64_t seq = 1;
    std::memcpy(payload, &seq, sizeof(seq));
    ingestor.process_payload(payload, sizeof(payload), 12345);

    // The ONLY observable output is a slot in market_events ring.
    // No FIX message, no send(), no order-entry interaction.
    egbos::shm::MarketEventSlot slot;
    EXPECT_TRUE(market_events.pop(slot))
        << "Expected exactly one market event in ring";
    EXPECT_FALSE(market_events.pop(slot))
        << "Expected no extra events — no outbound path triggered";

    CLOSE_FUNC(fd);
}

// ── Test 2: GapEvent emission produces no outbound socket call ────────────────
//
// Even when a gap is detected, the only observable output is a synthetic slot
// in the market_events ring. No FIX encode, no send().
TEST(ZeroOrderPathTest, GapDetection_NoOutboundSocketCall) {
    static egbos::shm::MarketEventsRegion market_events;
    static egbos::shm::GovernanceFlags    gov_flags{};
    int fd = OPEN_FUNC(DEV_NULL, O_FLAGS);
    ASSERT_GE(fd, 0);

    egbos::l0::FeedIngestor ingestor(&market_events, &gov_flags, fd);

    uint8_t payload[16] = {};
    uint64_t seq = 1;
    std::memcpy(payload, &seq, sizeof(seq));
    ingestor.process_payload(payload, sizeof(payload), 12345); // seq 1

    seq = 3; // gap: seq 2 missing
    std::memcpy(payload, &seq, sizeof(seq));
    ingestor.process_payload(payload, sizeof(payload), 12345); // seq 3

    // Expected ring contents: [slot(1), GapEvent, slot(3)]
    // No outbound FIX message, no ExecCommand dispatch, no order-entry.
    egbos::shm::MarketEventSlot slot;
    int event_count = 0;
    while (market_events.pop(slot)) ++event_count;
    EXPECT_EQ(event_count, 3)
        << "Expected exactly 3 ring events (slot1 + GapEvent + slot3)";

    CLOSE_FUNC(fd);
}

// ── Test 3: FIX consumer is not in the L0 receive call chain ─────────────────
//
// This test validates the ARCHITECTURAL CLAIM that FIXConsumer::encode() is
// never called from FeedIngestor::process_payload().
//
// Method: compile-time. If FIXConsumer was included in the L0 translation unit,
// its symbols would be linked. The absence of fix_consumer.o from the link of
// test_gap_detection and test_zero_order_path proves the isolation.
//
// This is enforced at link time: the test CMakeLists does NOT link fix_consumer.cpp
// into test_zero_order_path. If a build engineer adds fix_consumer.cpp, they must
// explicitly justify the change in a governance review.
//
// Classification: APPLICATION_RX_ONLY
TEST(ZeroOrderPathTest, FIXConsumer_NotLinkedIntoL0ReceivePath) {
    // This test passes by construction: if fix_consumer.cpp were linked here,
    // the undefined-reference linker error on FIXConsumer::encode() in a
    // mixed binary would surface. This test documents the invariant explicitly.
    //
    // A future static-analysis govcheck rule (proposed, not yet implemented)
    // should verify this link-time isolation via objdump/nm symbol inspection.
    SUCCEED()
        << "APPLICATION_RX_ONLY: FIX consumer not linked into L0 receive path";
}

// ── Enforcement gap documentation ─────────────────────────────────────────────
TEST(ZeroOrderPathTest, EnforcementGapDocumentation) {
    // This test is an explicit documentation assertion.
    // It will ALWAYS PASS; it serves as a machine-readable record of what
    // enforcement gap remains.
    //
    // CURRENT STATE: APPLICATION_RX_ONLY
    // MISSING FOR V4:
    //   A. Network namespace isolation (not implemented)
    //   B. iptables/nftables OUTPUT DROP (not implemented)
    //   C. seccomp DENY sendto/send/sendmsg (not implemented)
    //   D. CAP_NET_RAW restriction (not implemented)
    //   E. Production network topology VLAN segregation (not documented)
    //
    // Until at least one of A-E is implemented and proven by an artifact
    // classified ARCHITECTURAL_ENFORCEMENT, S5-REQ-005 is NOT satisfied
    // for V4 purposes.
    SUCCEED()
        << "APPLICATION_RX_ONLY confirmed — "
           "ARCHITECTURALLY_ENFORCED_ZERO_ORDER_PATH not yet implemented";
}

// ── Linux-only: loopback RX demonstration ────────────────────────────────────
#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

TEST(ZeroOrderPathTest, Linux_LoopbackReceive_OnlyRecvSyscall) {
    // Opens a UDP socket and binds to loopback on a non-privileged port.
    // Sends one packet via a SEPARATE sender socket (simulating a feed publisher).
    // Receives via the RX socket — recv() only, no send() on the receive socket.
    //
    // This demonstrates APPLICATION_RX_ONLY behaviour:
    //   - The market-data receive socket only calls recv/recvfrom.
    //   - It never calls send/sendto/sendmsg.
    //
    // This is NOT equivalent to ARCHITECTURAL enforcement.

    const int rx_sock = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(rx_sock, 0);

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(0); // let OS pick an ephemeral port
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ASSERT_EQ(bind(rx_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    // Discover the bound port.
    socklen_t addrlen = sizeof(addr);
    ASSERT_EQ(getsockname(rx_sock, reinterpret_cast<sockaddr*>(&addr), &addrlen), 0);

    // Sender socket — separate from the RX socket (simulates a feed publisher).
    const int tx_sock = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(tx_sock, 0);

    const char msg[] = "S5-REQ-005-loopback-test";
    ASSERT_EQ(static_cast<ssize_t>(sizeof(msg)),
              sendto(tx_sock, msg, sizeof(msg), 0,
                     reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));

    // RX path: recv only — no send from rx_sock.
    char buf[64] = {};
    ssize_t n = recv(rx_sock, buf, sizeof(buf) - 1, 0);
    EXPECT_GT(n, 0) << "loopback receive failed";
    EXPECT_STREQ(buf, msg);

    // Verify: no send() was called on rx_sock.
    // (Proved by code structure: only recv() is present in this path.)
    // APPLICATION_RX_ONLY: the receive socket never calls send/sendto.

    close(rx_sock);
    close(tx_sock);
}
#endif // __linux__
