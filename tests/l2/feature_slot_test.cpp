#include "core/shm_layout.h"
#include "l2/feature_publisher.h"
#include "l2/feature_reader.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

using namespace egbos;
static shm::FeatureRegion region;

static void test_layout() {
    static_assert(sizeof(shm::FeatureSlot) == 512);
    static_assert(alignof(shm::FeatureSlot) == 64);
    static_assert(offsetof(shm::FeatureSlot, payload) == 64);
    static_assert(sizeof(shm::FeatureSlotHeader) == 64);
}

static void test_basic() {
    l2::FeaturePublisher pub(&region);
    l2::FeatureReader rd(&region);
    l2::FeatureSlotView v;

    assert(!rd.try_read(3, v));                              // never published
    assert(pub.publish(3, 42, 1000, 0, 0, nullptr, 0));
    assert(rd.try_read(3, v));
    assert(v.instrument_id == 3 && v.source_seq == 42 && v.validity_mask == 0);
    assert(v.schema_version == l2::kFeatureSlotSchemaVersion);

    uint8_t buf[8] = {1,2,3,4,5,6,7,8};
    assert(pub.publish(3, 43, 1001, 0x1, 1, buf, 8));
    assert(rd.try_read(3, v) && v.payload_bytes == 8 && v.payload[7] == 8 && v.payload[8] == 0);

    uint8_t big[shm::kFeatureSlotPayloadBytes + 1]{};
    assert(!pub.publish(3, 1, 1, 0, 0, big, sizeof big));    // too large
    assert(!pub.publish(3, 1, 1, 0, 0, nullptr, 4));         // null with bytes
}

static void test_rejections() {
    l2::FeaturePublisher pub(&region);
    l2::FeatureReader rd(&region);
    l2::FeatureSlotView v;

    assert(!pub.publish(shm::kMaxInstruments, 1, 1, 0, 0, nullptr, 0));
    assert(!rd.try_read(shm::kMaxInstruments, v));
    assert(!l2::FeatureReader(nullptr).try_read(0, v));

    // schema mismatch
    assert(region.slots[5].publish(5, 99, 1, 1, 0, 0, nullptr, 0));
    assert(!rd.try_read(5, v));

    // instrument_id aliasing: slot 6 claims to be instrument 7
    assert(region.slots[6].publish(7, l2::kFeatureSlotSchemaVersion, 1, 1, 0, 0, nullptr, 0));
    assert(!rd.try_read(6, v));
}

static void test_stress() {
    l2::FeaturePublisher pub(&region);
    l2::FeatureReader rd(&region);
    std::atomic<bool> stop{false};
    constexpr uint32_t kId = 10;

    std::thread w([&] {
        uint8_t buf[shm::kFeatureSlotPayloadBytes];
        for (uint64_t i = 1; !stop.load(std::memory_order_relaxed); ++i) {
            for (auto& b : buf) b = static_cast<uint8_t>(i);
            pub.publish(kId, i, i, 0, 0, buf, sizeof buf);
        }
    });

    uint64_t ok = 0;
    for (int n = 0; n < 5'000'000; ++n) {
        l2::FeatureSlotView v;
        if (!rd.try_read(kId, v)) continue;
        ++ok;
        for (uint32_t k = 0; k < v.payload_bytes; ++k)
            assert(v.payload[k] == static_cast<uint8_t>(v.source_seq));  // torn read => fail
    }
    stop = true; w.join();
    std::printf("stress: %llu consistent reads\n", (unsigned long long)ok);
    assert(ok > 0);
}

int main() {
    test_layout(); test_basic(); test_rejections(); test_stress();
    std::puts("feature_slot_test: PASS");
}
