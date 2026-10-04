#pragma once

#include <cstddef>
#include <optional>
#include <type_traits>

#include <ecl/ring_buffer.hpp>
#include <ecl/sensor_reading.hpp>

namespace ecl {

/// Simple moving average over the last `Window` samples.
///
/// Guarantees:
///  - No heap allocation: the window is a RingBuffer<T, Window>.
///  - Amortized O(1) per sample: a running sum is updated incrementally, and
///    recomputed exactly from the window once every `Window` samples so that
///    rounding errors cannot build up over a long run.
///  - Until `Window` samples have arrived, the average is taken over the
///    samples received so far (use full() to know when the filter has warmed up).
///  - Non-finite values (NaN, +/- infinity) are rejected: add() returns
///    false and the filter is unchanged.
///  - With no samples, value() returns std::nullopt.
template <std::size_t Window, typename T = float, typename Acc = double>
class MovingAverage {
    static_assert(Window > 0, "MovingAverage window must be at least 1");
    static_assert(std::is_floating_point_v<T>,
                  "MovingAverage value type must be floating point");
    static_assert(std::is_floating_point_v<Acc>,
                  "MovingAverage accumulator type must be floating point");

public:
    using value_type = T;
    using size_type = std::size_t;

    [[nodiscard]] static constexpr size_type window() noexcept { return Window; }

    /// Adds a sample. Returns false (and ignores it) if it is not finite.
    bool add(T value) noexcept {
        if (!is_finite(value)) {
            return false;
        }
        if (samples_.full()) {
            sum_ -= static_cast<Acc>(samples_.front());  // oldest leaves
        }
        samples_.push(value);
        sum_ += static_cast<Acc>(value);

        if (++adds_since_resync_ >= Window) {
            resync();
        }
        return true;
    }

    /// Adds a reading's value. Invalid readings are ignored (returns false).
    bool add(const SensorReading& reading) noexcept {
        return reading.is_valid() && add(static_cast<T>(reading.value()));
    }

    /// Current average, or std::nullopt if no sample has been added.
    [[nodiscard]] std::optional<T> value() const noexcept {
        if (samples_.empty()) {
            return std::nullopt;
        }
        return static_cast<T>(sum_ / static_cast<Acc>(samples_.size()));
    }

    /// Number of samples currently in the window (at most Window).
    [[nodiscard]] constexpr size_type count() const noexcept {
        return samples_.size();
    }
    [[nodiscard]] constexpr bool empty() const noexcept {
        return samples_.empty();
    }
    /// True once the window holds Window samples.
    [[nodiscard]] constexpr bool full() const noexcept {
        return samples_.full();
    }

    /// Forgets all samples.
    void reset() noexcept {
        samples_.clear();
        sum_ = Acc{0};
        adds_since_resync_ = 0;
    }

private:
    // x - x is 0 for every finite x, and NaN for NaN and +/- infinity.
    static constexpr bool is_finite(T x) noexcept { return x - x == T{0}; }

    // Recomputes the sum exactly from the window contents.
    void resync() noexcept {
        sum_ = Acc{0};
        for (size_type i = 0; i < samples_.size(); ++i) {
            sum_ += static_cast<Acc>(samples_[i]);
        }
        adds_since_resync_ = 0;
    }

    RingBuffer<T, Window> samples_{};
    Acc sum_{0};
    size_type adds_since_resync_{0};
};

}  // namespace ecl
