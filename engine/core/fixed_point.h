// engine/core/fixed_point.h
// EGBOS ENGINE_V3 — Fixed-Point Price Arithmetic
//
// Specification  : AD-004 (Fixed-Point Math Mandate)
// Claims         : CLAIM-FEATURE-001 (determinism)
//
// HFT constraint : __builtin_add_overflow used for branchless, hardware-level
//                  overflow detection. constexpr evaluation everywhere possible.

#pragma once

#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace egbos::core {

// PRICE_SCALE = 100,000 per AD-004
inline constexpr int64_t kPriceScale = 100000;

class Price {
public:
    constexpr Price() noexcept : value_(0) {}
    constexpr explicit Price(int64_t raw_value) noexcept : value_(raw_value) {}

    // Factory from double for compile-time constants (e.g. Price::from_double(1.5))
    // Not recommended on hot path due to float non-determinism, but useful for init.
    static constexpr Price from_double(double v) noexcept {
        return Price(static_cast<int64_t>(v * kPriceScale));
    }

    constexpr int64_t raw() const noexcept { return value_; }

    // Arithmetic operators with overflow protection
    constexpr Price operator+(const Price& other) const {
        int64_t result;
        if (__builtin_add_overflow(value_, other.value_, &result)) {
            throw std::overflow_error("Price addition overflow");
        }
        return Price(result);
    }

    constexpr Price operator-(const Price& other) const {
        int64_t result;
        if (__builtin_sub_overflow(value_, other.value_, &result)) {
            throw std::overflow_error("Price subtraction overflow");
        }
        return Price(result);
    }

    constexpr Price operator*(int64_t multiplier) const {
        int64_t result;
        if (__builtin_mul_overflow(value_, multiplier, &result)) {
            throw std::overflow_error("Price multiplication overflow");
        }
        return Price(result);
    }
    
    // Division by scalar
    constexpr Price operator/(int64_t divisor) const {
        if (divisor == 0) [[unlikely]] {
            throw std::domain_error("Price division by zero");
        }
        return Price(value_ / divisor);
    }

    // Multiplication of two prices (requires scaling back)
    constexpr Price multiply_price(const Price& other) const {
        // Use __int128_t for intermediate to prevent overflow before scale down
        __int128_t intermediate = static_cast<__int128_t>(value_) * other.value_;
        return Price(static_cast<int64_t>(intermediate / kPriceScale));
    }

    // Comparison operators
    constexpr bool operator==(const Price& other) const noexcept { return value_ == other.value_; }
    constexpr bool operator!=(const Price& other) const noexcept { return value_ != other.value_; }
    constexpr bool operator<(const Price& other) const noexcept { return value_ < other.value_; }
    constexpr bool operator<=(const Price& other) const noexcept { return value_ <= other.value_; }
    constexpr bool operator>(const Price& other) const noexcept { return value_ > other.value_; }
    constexpr bool operator>=(const Price& other) const noexcept { return value_ >= other.value_; }

private:
    int64_t value_;
};

} // namespace egbos::core
