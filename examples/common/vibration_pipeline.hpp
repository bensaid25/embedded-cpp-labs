#pragma once

// A hardware-independent vibration pipeline built from the ecl modules.
//
//   SensorReading -> MovingAverage (estimates the slow DC part, e.g. gravity)
//                 -> AC value = sample - DC estimate
//                 -> Statistics  (RMS and peak over a window of samples)
//                 -> RingBuffer  (history of recent window RMS values)
//                 -> ThresholdDetector (alert when the RMS is too high)
//
// The same header runs on the host (unit tests, demo) and on the ESP32.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include <ecl/moving_average.hpp>
#include <ecl/ring_buffer.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/statistics.hpp>
#include <ecl/threshold_detector.hpp>

namespace demo {

/// Summary of one completed window of samples.
struct WindowResult {
    std::size_t index;  ///< 0, 1, 2, ... counts completed windows
    float rms;          ///< RMS of the AC part, same unit as the input
    float peak;         ///< largest absolute AC value in the window
    ecl::ThresholdState state;
    bool state_changed;  ///< true if this window changed the alert state
};

/// WindowSamples: samples per result window.
/// DcWindow:      length of the moving average that estimates the DC part.
///                It should be much shorter than WindowSamples.
/// History:       how many recent window RMS values to remember.
///
/// No heap allocation. Invalid readings are skipped (and counted).
template <std::size_t WindowSamples = 200, std::size_t DcWindow = 32,
          std::size_t History = 8>
class VibrationPipeline {
    static_assert(WindowSamples > 0, "WindowSamples must be at least 1");
    static_assert(DcWindow > 0, "DcWindow must be at least 1");
    static_assert(History > 0, "History must be at least 1");

public:
    /// alert_rms:  alert when a window's RMS rises above this value.
    /// hysteresis: the RMS must fall to alert_rms - hysteresis to clear it.
    /// Returns std::nullopt for an invalid configuration.
    [[nodiscard]] static std::optional<VibrationPipeline> make(
        float alert_rms, float hysteresis) {
        const auto detector =
            ecl::ThresholdDetector<>::make(0.0f, alert_rms, hysteresis);
        if (!detector) {
            return std::nullopt;
        }
        return VibrationPipeline{*detector};
    }

    /// Feeds one reading. Returns a result when it completes a window.
    std::optional<WindowResult> add(const ecl::SensorReading& reading) {
        if (!reading.is_valid()) {
            ++skipped_;
            return std::nullopt;
        }

        dc_.add(reading);
        const auto dc = dc_.value();
        if (!dc_.full() || !dc) {
            return std::nullopt;  // still warming up the DC estimate
        }

        stats_.add(reading.value() - *dc);
        if (stats_.count() < WindowSamples) {
            return std::nullopt;
        }

        const float rms = stats_.rms().value_or(0.0f);
        const float peak = std::max(std::abs(stats_.min().value_or(0.0f)),
                                    std::abs(stats_.max().value_or(0.0f)));
        history_.push(rms);
        const bool changed = detector_.update(rms);
        stats_.reset();

        return WindowResult{window_index_++, rms, peak, detector_.state(),
                            changed};
    }

    /// RMS values of the most recent windows, oldest first.
    [[nodiscard]] const ecl::RingBuffer<float, History>& history() const {
        return history_;
    }
    /// Number of invalid readings skipped so far.
    [[nodiscard]] std::size_t skipped() const { return skipped_; }
    [[nodiscard]] ecl::ThresholdState state() const {
        return detector_.state();
    }

private:
    explicit VibrationPipeline(const ecl::ThresholdDetector<>& detector)
        : detector_{detector} {}

    ecl::MovingAverage<DcWindow> dc_{};
    ecl::Statistics<> stats_{};
    ecl::RingBuffer<float, History> history_{};
    ecl::ThresholdDetector<> detector_;
    std::size_t window_index_{0};
    std::size_t skipped_{0};
};

}  // namespace demo
