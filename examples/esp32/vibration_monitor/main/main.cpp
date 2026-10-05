// Vibration monitor: MPU6050 -> ecl pipeline -> serial output.
//
// Samples the accelerometer at 200 Hz, removes gravity, and every 200 samples
// (one second) prints the RMS and peak of the vibration, the alert state, and
// how long the processing took. Shake the board to trigger the alert.

#include <cmath>
#include <cstdint>
#include <cstdio>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <ecl/sensor_reading.hpp>
#include <ecl/statistics.hpp>
#include <vibration_pipeline.hpp>

#include "mpu6050.hpp"

namespace {

// ---- settings you may need to change --------------------------------------
constexpr gpio_num_t kSdaPin = GPIO_NUM_21;  // I2C data  (ESP32 DevKit default)
constexpr gpio_num_t kSclPin = GPIO_NUM_22;  // I2C clock (ESP32 DevKit default)
constexpr std::uint8_t kMpuAddress = 0x68;   // 0x69 if the AD0 pin is high
constexpr std::uint32_t kSamplePeriodMs = 5; // 200 Hz
constexpr float kAlertRmsG = 0.05f;          // alert above this RMS (in g)
constexpr float kHysteresisG = 0.02f;        // clears below 0.03 g
// ----------------------------------------------------------------------------

constexpr std::size_t kWindowSamples = 200;  // one result per second
using Pipeline = demo::VibrationPipeline<kWindowSamples, 32, 8>;

const char* state_name(ecl::ThresholdState state) {
    switch (state) {
        case ecl::ThresholdState::Normal: return "NORMAL";
        case ecl::ThresholdState::Low:    return "LOW";
        case ecl::ThresholdState::High:   return "ALERT";
    }
    return "?";
}

std::uint32_t now_ms() {
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}

}  // namespace

extern "C" void app_main(void) {
    // ---- I2C bus and sensor ------------------------------------------------
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = kSdaPin;
    bus_config.scl_io_num = kSclPin;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus = nullptr;
    if (i2c_new_master_bus(&bus_config, &bus) != ESP_OK) {
        std::puts("ERROR: could not create the I2C bus");
        return;
    }

    Mpu6050 mpu;
    if (mpu.init(bus, kMpuAddress) != ESP_OK) {
        std::puts("ERROR: MPU6050 not responding. Check wiring and address.");
        return;
    }
    std::uint8_t who_am_i = 0;
    if (mpu.read_who_am_i(who_am_i) == ESP_OK) {
        std::printf("MPU6050 WHO_AM_I = 0x%02X (0x68 expected)\n", who_am_i);
    }

    // ---- pipeline ------------------------------------------------------------
    auto pipeline = Pipeline::make(kAlertRmsG, kHysteresisG);
    if (!pipeline) {
        std::puts("ERROR: invalid pipeline configuration");
        return;
    }

    std::printf("Sampling at %u Hz. Shake the board to trigger the alert.\n",
                static_cast<unsigned>(1000 / kSamplePeriodMs));
    std::puts("window  rms(g)  peak(g)  state   read_us(avg/max)  proc_us(avg/max)  errors");

    ecl::Statistics<> read_time_us;  // time spent in the I2C read
    ecl::Statistics<> proc_time_us;  // time spent in the pipeline
    unsigned read_errors = 0;

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kSamplePeriodMs));

        // Read the sensor; a failed read becomes an invalid reading.
        const std::int64_t t0 = esp_timer_get_time();
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        const bool ok = mpu.read_accel_g(x, y, z) == ESP_OK;
        const std::int64_t t1 = esp_timer_get_time();

        const ecl::SensorReading reading =
            ok ? ecl::SensorReading{std::sqrt(x * x + y * y + z * z), now_ms()}
               : ecl::SensorReading::invalid(now_ms());
        if (!ok) {
            ++read_errors;
        }

        // Run the pipeline and time it.
        const auto result = pipeline->add(reading);
        const std::int64_t t2 = esp_timer_get_time();

        read_time_us.add(static_cast<float>(t1 - t0));
        proc_time_us.add(static_cast<float>(t2 - t1));

        // Print once per window, so the serial output does not disturb timing.
        if (result) {
            std::printf("%6u  %6.3f  %7.3f  %-6s  %6.0f/%-6.0f    %6.1f/%-6.0f    %u%s\n",
                        static_cast<unsigned>(result->index),
                        static_cast<double>(result->rms),
                        static_cast<double>(result->peak),
                        state_name(result->state),
                        static_cast<double>(read_time_us.mean().value_or(0.0f)),
                        static_cast<double>(read_time_us.max().value_or(0.0f)),
                        static_cast<double>(proc_time_us.mean().value_or(0.0f)),
                        static_cast<double>(proc_time_us.max().value_or(0.0f)),
                        read_errors,
                        result->state_changed ? "  <-- state changed" : "");
            read_time_us.reset();
            proc_time_us.reset();
        }
    }
}
