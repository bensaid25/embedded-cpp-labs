#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <type_traits>

#include <ecl/sensor_reading.hpp>

namespace ecl {

/// Streaming statistics: count, min, max, mean and RMS.
///
/// Guarantees:
///  - Constant memory, no heap allocation, never throws.
///  - O(1) per sample.
///  - Sums are kept in the wider type Acc (double by default), so adding
///    many small floats does not drift the way a float sum would.
///  - Non-finite values (NaN, +/- infinity) are rejected: add() returns
///    false and the statistics are unchanged.
///  - With no samples, min/max/mean/rms return std::nullopt.
template <typename T = float, typename Acc = double>
class Statistics {
    static_assert(std::is_floating_point_v<T>,
                  "Statistics value type must be floating point");
    static_assert(std::is_floating_point_v<Acc>,
                  "Statistics accumulator type must be floating point");

public:
    using value_type = T;
    using size_type = std::size_t;

    /// Adds a sample. Returns false (and ignores it) if it is not finite.
    bool add(T value) noexcept {
        if (!is_finite(value)) {
            return false;
        }
        if (count_ == 0) {
            min_ = value;
            max_ = value;
        } else {
            if (value < min_) {
                min_ = value;
            }
            if (value > max_) {
                max_ = value;
            }
        }
        const Acc v = static_cast<Acc>(value);
        sum_ += v;
        sum_sq_ += v * v;
        ++count_;
        return true;
    }

    /// Adds a reading's value. Invalid readings are ignored (returns false).
    bool add(const SensorReading& reading) noexcept {
        return reading.is_valid() && add(static_cast<T>(reading.value()));
    }

    [[nodiscard]] constexpr size_type count() const noexcept { return count_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return count_ == 0; }

    [[nodiscard]] std::optional<T> min() const noexcept {
        if (empty()) {
            return std::nullopt;
        }
        return min_;
    }

    [[nodiscard]] std::optional<T> max() const noexcept {
        if (empty()) {
            return std::nullopt;
        }
        return max_;
    }

    [[nodiscard]] std::optional<T> mean() const noexcept {
        if (empty()) {
            return std::nullopt;
        }
        return static_cast<T>(sum_ / static_cast<Acc>(count_));
    }

    /// Root mean square: sqrt(mean of squares).
    [[nodiscard]] std::optional<T> rms() const noexcept {
        if (empty()) {
            return std::nullopt;
        }
        return static_cast<T>(std::sqrt(sum_sq_ / static_cast<Acc>(count_)));
    }

    /// Forgets all samples.
    void reset() noexcept { *this = Statistics{}; }

private:
    // x - x is 0 for every finite x, and NaN for NaN and +/- infinity.
    static constexpr bool is_finite(T x) noexcept { return x - x == T{0}; }

    size_type count_{0};
    T min_{0};
    T max_{0};
    Acc sum_{0};
    Acc sum_sq_{0};
};

}  // namespace ecl
