// tests/governance/fixtures/violation_GC_E_045.cpp
#include <atomic>
#include <cstdint>

namespace egbos::signal {

struct CrossPlaneView {
    std::atomic<int64_t> x_l_{0};
    std::atomic<int64_t> order_tag_{0};
};

int64_t direct_read_VIOLATION(const CrossPlaneView& view) {
    // GC-E-045: direct x_l_ read from L4/L5 without CROSS_PLANE_READ
    int64_t val = view.x_l_.load(std::memory_order_acquire);
    return val;
}

}  // namespace egbos::signal
