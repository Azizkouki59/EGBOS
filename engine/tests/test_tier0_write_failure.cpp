#include <gtest/gtest.h>

#include "engine/l0/ingestor.h"
#include "engine/core/shm_layout.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>

using namespace egbos::l0;
using namespace egbos::shm;

TEST(Tier0WriteGateTest, WriteFailurePreventsSpscPublication) {
    static MarketEventsRegion ring{};
    GovernanceFlags gov{};

    const int fd = ::open("/dev/full", O_WRONLY);
    ASSERT_GE(fd, 0);

    FeedIngestor ingestor(&ring, &gov, fd);

    const char* json =
        R"({"e":"depthUpdate","U":100,"u":100,"b":[["1.0","1.0"]],"a":[]})";

    ingestor.process_payload(
        reinterpret_cast<const uint8_t*>(json),
        std::strlen(json),
        123456789ULL
    );

    MarketEventSlot slot{};
    const bool popped = ring.pop(slot);

    EXPECT_FALSE(popped)
        << "Tier-0 write failure must prevent SPSC publication";

    ::close(fd);
}

TEST(Tier0WriteGateTest, DisabledTier0ModeStillPublishesForDevelopmentTests) {
    static MarketEventsRegion ring{};
    GovernanceFlags gov{};

    FeedIngestor ingestor(&ring, &gov, -1);

    const char* json =
        R"({"e":"depthUpdate","U":100,"u":100,"b":[["1.0","1.0"]],"a":[]})";

    ingestor.process_payload(
        reinterpret_cast<const uint8_t*>(json),
        std::strlen(json),
        123456789ULL
    );

    MarketEventSlot slot{};
    const bool popped = ring.pop(slot);

    EXPECT_TRUE(popped)
        << "Explicit tier0_fd=-1 test mode must retain existing behavior";
}
