// A complete host-side pipeline using every module of the library:
//
//   SensorReading -> MovingAverage (smooth) -> ThresholdDetector (alert)
//                 -> Statistics    (summary of the raw valid readings)
//
// The signal is simulated: 20 degrees with a little noise, a hot spell of
// 30 degrees from sample 15 to 24, and one failed reading at sample 7.

#include <cstdint>
#include <cstdio>

#include <ecl/moving_average.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/statistics.hpp>
#include <ecl/threshold_detector.hpp>

namespace {

// Tiny deterministic noise source: the output is identical on every run.
float noise(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return (static_cast<float>(state >> 16) / 65535.0f - 0.5f) * 1.6f;  // +/- 0.8
}

const char* name(ecl::ThresholdState state) {
    switch (state) {
        case ecl::ThresholdState::Normal: return "NORMAL";
        case ecl::ThresholdState::Low:    return "LOW";
        case ecl::ThresholdState::High:   return "HIGH";
    }
    return "?";
}

}  // namespace

int main() {
    ecl::MovingAverage<5> filter;
    ecl::Statistics<> raw_stats;

    // Alert above 25, or below 10; clear only 1 degree back inside the band.
    auto detector = ecl::ThresholdDetector<>::make(10.0f, 25.0f, 1.0f);
    if (!detector) {
        std::puts("invalid detector configuration");
        return 1;
    }

    std::uint32_t seed = 42;
    std::puts("  t(ms)     raw     avg  state");

    for (std::uint32_t i = 0; i < 40; ++i) {
        const std::uint32_t t = i * 100;
        const float truth = (i >= 15 && i < 25) ? 30.0f : 20.0f;

        // Sample 7 simulates a sensor fault.
        const ecl::SensorReading reading =
            (i == 7) ? ecl::SensorReading::invalid(t)
                     : ecl::SensorReading{truth + noise(seed), t};

        raw_stats.add(reading);  // invalid readings are skipped automatically
        filter.add(reading);

        const auto average = filter.value();
        if (!average) {
            continue;  // nothing to show before the first valid sample
        }
        const bool changed = detector->update(*average);

        if (reading.is_valid()) {
            std::printf("%7u  %6.2f  %6.2f  %-6s", static_cast<unsigned>(t),
                        static_cast<double>(reading.value()),
                        static_cast<double>(*average), name(detector->state()));
        } else {
            std::printf("%7u  %6s  %6.2f  %-6s", static_cast<unsigned>(t),
                        "fault", static_cast<double>(*average),
                        name(detector->state()));
        }
        std::puts(changed ? "  <-- state changed" : "");
    }

    std::printf("\nraw readings: %zu valid\n", raw_stats.count());
    std::printf("  min  %.2f\n  max  %.2f\n  mean %.2f\n  rms  %.2f\n",
                static_cast<double>(raw_stats.min().value_or(0.0f)),
                static_cast<double>(raw_stats.max().value_or(0.0f)),
                static_cast<double>(raw_stats.mean().value_or(0.0f)),
                static_cast<double>(raw_stats.rms().value_or(0.0f)));
    return 0;
}
