// Runs the vibration pipeline on a synthetic accelerometer signal:
// 3 s of rest, 3 s of vibration, 3 s of rest, sampled at 200 Hz.
// The same pipeline header is used on the ESP32 (examples/esp32).

#include <cmath>
#include <cstdint>
#include <cstdio>

#include <ecl/sensor_reading.hpp>
#include <vibration_pipeline.hpp>

namespace {

const char* name(ecl::ThresholdState state) {
    switch (state) {
        case ecl::ThresholdState::Normal: return "NORMAL";
        case ecl::ThresholdState::Low:    return "LOW";
        case ecl::ThresholdState::High:   return "ALERT";
    }
    return "?";
}

}  // namespace

int main() {
    constexpr double kPi = 3.14159265358979323846;
    constexpr std::uint32_t kSamplePeriodMs = 5;  // 200 Hz

    // Alert above 0.2 g RMS, clear below 0.15 g RMS.
    auto pipeline = demo::VibrationPipeline<200, 32, 8>::make(0.2f, 0.05f);
    if (!pipeline) {
        std::puts("invalid pipeline configuration");
        return 1;
    }

    std::puts("window   rms(g)  peak(g)  state");
    for (std::uint32_t i = 0; i < 9 * 200; ++i) {
        const bool vibrating = i >= 3 * 200 && i < 6 * 200;
        // 1 g of gravity, plus a 25 Hz vibration of 0.5 g during the middle part.
        const float amplitude = vibrating ? 0.5f : 0.0f;
        const float g = 1.0f + amplitude * static_cast<float>(std::sin(
                                   2.0 * kPi * 25.0 * i * kSamplePeriodMs / 1000.0));

        const ecl::SensorReading reading{g, i * kSamplePeriodMs};
        if (const auto result = pipeline->add(reading)) {
            std::printf("%6zu  %7.3f  %7.3f  %-6s%s\n", result->index,
                        static_cast<double>(result->rms),
                        static_cast<double>(result->peak), name(result->state),
                        result->state_changed ? "  <-- state changed" : "");
        }
    }
    return 0;
}
