#pragma once

#include <cstdint>
#include <optional>
#include <type_traits>

#include <ecl/detail/finite.hpp>
#include <ecl/sensor_reading.hpp>

namespace ecl {

enum class ThresholdState : std::uint8_t {
    Normal,  ///< low <= value <= high (or not yet cleared, see hysteresis)
    Low,     ///< value dropped below the low limit
    High     ///< value rose above the high limit
};

/// Detects values outside [low, high], with optional hysteresis.
///
/// Rules (h = hysteresis):
///  - Normal -> High when value > high; Normal -> Low when value < low.
///    A value exactly on a limit is still Normal.
///  - High returns to Normal only when value <= high - h.
///    Low  returns to Normal only when value >= low  + h.
///    This stops the state flickering when a noisy value hovers at a limit.
///  - If a value jumps straight across the whole band, the state goes
///    directly from High to Low or from Low to High.
///  - Non-finite values (NaN, +/- infinity) and invalid readings are ignored.
///
/// Valid configuration: finite limits, low <= high, 0 <= h <= high - low.
/// Because there are no exceptions, construct with make(), which returns
/// std::nullopt for an invalid configuration.
///
/// No heap allocation, never throws, O(1) per sample.
template <typename T = float>
class ThresholdDetector {
    static_assert(std::is_floating_point_v<T>,
                  "ThresholdDetector value type must be floating point");

public:
    using value_type = T;

    [[nodiscard]] static constexpr std::optional<ThresholdDetector> make(
        T low, T high, T hysteresis = T{0}) noexcept {
        if (!detail::is_finite(low) || !detail::is_finite(high) ||
            !detail::is_finite(hysteresis)) {
            return std::nullopt;
        }
        if (low > high) {
            return std::nullopt;
        }
        if (hysteresis < T{0} || hysteresis > high - low) {
            return std::nullopt;
        }
        return ThresholdDetector{low, high, hysteresis};
    }

    /// Feeds one value. Returns true if the state changed because of it.
    constexpr bool update(T value) noexcept {
        if (!detail::is_finite(value)) {
            return false;
        }
        const ThresholdState previous = state_;
        switch (state_) {
            case ThresholdState::Normal:
                state_ = classify(value);
                break;
            case ThresholdState::High:
                if (value <= high_clear_) {
                    state_ = classify(value);
                }
                break;
            case ThresholdState::Low:
                if (value >= low_clear_) {
                    state_ = classify(value);
                }
                break;
        }
        return state_ != previous;
    }

    /// Feeds a reading's value. Invalid readings are ignored (returns false).
    constexpr bool update(const SensorReading& reading) noexcept {
        return reading.is_valid() && update(static_cast<T>(reading.value()));
    }

    [[nodiscard]] constexpr ThresholdState state() const noexcept {
        return state_;
    }
    [[nodiscard]] constexpr bool is_alarm() const noexcept {
        return state_ != ThresholdState::Normal;
    }
    [[nodiscard]] constexpr T low() const noexcept { return low_; }
    [[nodiscard]] constexpr T high() const noexcept { return high_; }
    [[nodiscard]] constexpr T hysteresis() const noexcept { return hysteresis_; }

    /// Back to Normal, as if no value had been seen.
    constexpr void reset() noexcept { state_ = ThresholdState::Normal; }

private:
    constexpr ThresholdDetector(T low, T high, T hysteresis) noexcept
        : low_{low},
          high_{high},
          hysteresis_{hysteresis},
          low_clear_{low + hysteresis},
          high_clear_{high - hysteresis} {}

    // State a value belongs to when judged against the raw limits.
    [[nodiscard]] constexpr ThresholdState classify(T value) const noexcept {
        if (value > high_) {
            return ThresholdState::High;
        }
        if (value < low_) {
            return ThresholdState::Low;
        }
        return ThresholdState::Normal;
    }

    T low_;
    T high_;
    T hysteresis_;
    T low_clear_;   // value at or above which Low may clear
    T high_clear_;  // value at or below which High may clear
    ThresholdState state_{ThresholdState::Normal};
};

}  // namespace ecl
