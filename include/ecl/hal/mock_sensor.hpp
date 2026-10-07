#pragma once

#include <cstdint>
#include <limits>

#include <ecl/hal/sensor.hpp>
#include <ecl/sensor_reading.hpp>

namespace ecl::hal {

/// Settings for MockSensor. Every field has a harmless default.
///
/// Build one by chaining the with_*() functions:
///
///     MockSensor sensor{MockSensorConfig{}
///                           .with_baseline(20.0f)
///                           .with_noise(0.5f)
///                           .with_spikes(50, 8.0f)
///                           .with_faults(30)};
struct MockSensorConfig {
    float baseline = 20.0f;         // the "true" value
    float noise_amplitude = 0.0f;   // uniform noise in +/- noise_amplitude
    float drift_per_second = 0.0f;  // slow linear change of the baseline
    std::uint32_t spike_every = 0;  // every Nth sample gets a spike (0 = never)
    float spike_magnitude = 0.0f;   // size of that spike
    std::uint32_t fault_every = 0;  // every Nth sample is reported invalid
    std::uint32_t nan_every = 0;    // every Nth sample reads NaN
    std::uint32_t seed = 1;         // same seed, same sequence

    [[nodiscard]] constexpr MockSensorConfig with_baseline(float v) const noexcept {
        MockSensorConfig c = *this;
        c.baseline = v;
        return c;
    }
    [[nodiscard]] constexpr MockSensorConfig with_noise(float amplitude) const noexcept {
        MockSensorConfig c = *this;
        c.noise_amplitude = amplitude;
        return c;
    }
    [[nodiscard]] constexpr MockSensorConfig with_drift(float per_second) const noexcept {
        MockSensorConfig c = *this;
        c.drift_per_second = per_second;
        return c;
    }
    [[nodiscard]] constexpr MockSensorConfig with_spikes(std::uint32_t every,
                                                         float magnitude) const noexcept {
        MockSensorConfig c = *this;
        c.spike_every = every;
        c.spike_magnitude = magnitude;
        return c;
    }
    [[nodiscard]] constexpr MockSensorConfig with_faults(std::uint32_t every) const noexcept {
        MockSensorConfig c = *this;
        c.fault_every = every;
        return c;
    }
    [[nodiscard]] constexpr MockSensorConfig with_nans(std::uint32_t every) const noexcept {
        MockSensorConfig c = *this;
        c.nan_every = every;
        return c;
    }
    [[nodiscard]] constexpr MockSensorConfig with_seed(std::uint32_t s) const noexcept {
        MockSensorConfig c = *this;
        c.seed = s;
        return c;
    }
};

/// A simulated sensor for host tests and demos. Deterministic: the same
/// config always produces the same readings, on every machine.
///
/// It can inject the things real sensors do: noise, slow drift, spikes,
/// failed reads and NaN values (which SensorReading turns into invalid).
/// Samples are counted from 1, so spike_every = 4 hits samples 4, 8, 12...
/// When a sample is both a fault and a spike, the fault wins.
///
/// No heap, no exceptions, no floating-point exceptions.
class MockSensor {
public:
    constexpr MockSensor() noexcept = default;
    explicit constexpr MockSensor(const MockSensorConfig& config) noexcept
        : config_{config}, rng_{config.seed} {}

    [[nodiscard]] SensorReading read(SensorReading::Timestamp now_ms) noexcept {
        const std::uint32_t n = ++count_;

        if (hits(config_.fault_every, n)) {
            return SensorReading::invalid(now_ms);
        }
        if (hits(config_.nan_every, n)) {
            return SensorReading{std::numeric_limits<float>::quiet_NaN(),
                                 now_ms};
        }

        float value = config_.baseline +
                      config_.drift_per_second *
                          (static_cast<float>(now_ms) / 1000.0f);
        if (config_.noise_amplitude != 0.0f) {
            value += config_.noise_amplitude * next_unit_noise();
        }
        if (hits(config_.spike_every, n)) {
            value += config_.spike_magnitude;
        }
        return SensorReading{value, now_ms};
    }

    /// Number of read() calls so far.
    [[nodiscard]] constexpr std::uint32_t samples_taken() const noexcept {
        return count_;
    }

private:
    [[nodiscard]] static constexpr bool hits(std::uint32_t every,
                                             std::uint32_t n) noexcept {
        return every != 0 && n % every == 0;
    }

    /// Next value in [-1, +1]. A small linear congruential generator: not
    /// random enough for statistics, but tiny, fast and fully reproducible.
    [[nodiscard]] constexpr float next_unit_noise() noexcept {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>(rng_ >> 16) / 32767.5f - 1.0f;
    }

    MockSensorConfig config_{};
    std::uint32_t rng_{1};
    std::uint32_t count_{0};
};

static_assert(is_sensor_v<MockSensor>, "MockSensor must satisfy the sensor contract");

}  // namespace ecl::hal
