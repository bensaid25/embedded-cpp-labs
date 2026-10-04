#pragma once

#include <cstdint>

#include <ecl/detail/finite.hpp>

namespace ecl {

/// One timestamped measurement from a sensor.
///
/// Guarantees:
///  - Trivially copyable, no heap allocation, never throws.
///  - A reading is valid only if the caller marks it valid AND the value is
///    finite (not NaN, not +/- infinity).
///  - An invalid reading always stores the value 0, so equality is well
///    defined and no garbage value can leak out.
class SensorReading {
public:
    using Value = float;
    using Timestamp = std::uint32_t;  // milliseconds

    /// Default-constructed readings are invalid, with value 0 and time 0.
    constexpr SensorReading() noexcept = default;

    constexpr SensorReading(Value value, Timestamp timestamp_ms,
                            bool valid = true) noexcept
        : value_{valid && detail::is_finite(value) ? value : Value{0}},
          timestamp_ms_{timestamp_ms},
          valid_{valid && detail::is_finite(value)} {}

    /// An invalid reading that still remembers when it was taken.
    [[nodiscard]] static constexpr SensorReading invalid(
        Timestamp timestamp_ms) noexcept {
        return SensorReading{Value{0}, timestamp_ms, false};
    }

    [[nodiscard]] constexpr Value value() const noexcept { return value_; }
    [[nodiscard]] constexpr Timestamp timestamp_ms() const noexcept {
        return timestamp_ms_;
    }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return valid_; }

    friend constexpr bool operator==(const SensorReading& a,
                                     const SensorReading& b) noexcept {
        return a.value_ == b.value_ && a.timestamp_ms_ == b.timestamp_ms_ &&
               a.valid_ == b.valid_;
    }
    friend constexpr bool operator!=(const SensorReading& a,
                                     const SensorReading& b) noexcept {
        return !(a == b);
    }

private:

    Value value_{0};
    Timestamp timestamp_ms_{0};
    bool valid_{false};
};

}  // namespace ecl
