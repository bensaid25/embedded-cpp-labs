#pragma once

// Minimal MPU6050 accelerometer driver for the ESP-IDF I2C master driver.
// Only what this example needs: wake the chip, set the range, read X/Y/Z.

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

class Mpu6050 {
public:
    /// Registers the sensor on an existing I2C bus, wakes it up and configures
    /// it (+/- 4 g range, 94 Hz low-pass filter).
    esp_err_t init(i2c_master_bus_handle_t bus, std::uint8_t address = 0x68);

    /// Reads the acceleration in g.
    esp_err_t read_accel_g(float& x, float& y, float& z);

    /// Reads the WHO_AM_I register (0x68 on a genuine MPU6050).
    esp_err_t read_who_am_i(std::uint8_t& id);

private:
    esp_err_t write_register(std::uint8_t reg, std::uint8_t value);
    esp_err_t read_registers(std::uint8_t reg, std::uint8_t* data,
                             std::size_t length);

    i2c_master_dev_handle_t device_{nullptr};
};
