#include "mpu6050.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr std::uint8_t kRegSmplrtDiv = 0x19;
constexpr std::uint8_t kRegConfig = 0x1A;
constexpr std::uint8_t kRegAccelConfig = 0x1C;
constexpr std::uint8_t kRegAccelXoutH = 0x3B;
constexpr std::uint8_t kRegPwrMgmt1 = 0x6B;
constexpr std::uint8_t kRegWhoAmI = 0x75;

constexpr std::uint8_t kAccelRange4g = 0x08;    // AFS_SEL = 1
constexpr std::uint8_t kDlpf94Hz = 0x02;        // DLPF_CFG = 2
constexpr float kLsbPerG = 8192.0f;             // sensitivity at +/- 4 g
constexpr int kTimeoutMs = 100;

}  // namespace

esp_err_t Mpu6050::init(i2c_master_bus_handle_t bus, std::uint8_t address) {
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = address;
    config.scl_speed_hz = 400000;

    esp_err_t err = i2c_master_bus_add_device(bus, &config, &device_);
    if (err != ESP_OK) {
        return err;
    }

    // Leave sleep mode (the chip starts asleep), then configure.
    err = write_register(kRegPwrMgmt1, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    err = write_register(kRegSmplrtDiv, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    err = write_register(kRegConfig, kDlpf94Hz);
    if (err != ESP_OK) {
        return err;
    }
    return write_register(kRegAccelConfig, kAccelRange4g);
}

esp_err_t Mpu6050::read_accel_g(float& x, float& y, float& z) {
    std::uint8_t raw[6] = {};
    const esp_err_t err = read_registers(kRegAccelXoutH, raw, sizeof(raw));
    if (err != ESP_OK) {
        return err;
    }
    // Each axis is a big-endian signed 16-bit value.
    const auto axis = [&raw](int index) {
        const auto high = static_cast<std::uint16_t>(raw[index * 2]);
        const auto low = static_cast<std::uint16_t>(raw[index * 2 + 1]);
        return static_cast<float>(static_cast<std::int16_t>((high << 8) | low)) /
               kLsbPerG;
    };
    x = axis(0);
    y = axis(1);
    z = axis(2);
    return ESP_OK;
}

esp_err_t Mpu6050::read_who_am_i(std::uint8_t& id) {
    return read_registers(kRegWhoAmI, &id, 1);
}

esp_err_t Mpu6050::write_register(std::uint8_t reg, std::uint8_t value) {
    const std::uint8_t buffer[2] = {reg, value};
    return i2c_master_transmit(device_, buffer, sizeof(buffer), kTimeoutMs);
}

esp_err_t Mpu6050::read_registers(std::uint8_t reg, std::uint8_t* data,
                                  std::size_t length) {
    return i2c_master_transmit_receive(device_, &reg, 1, data, length,
                                       kTimeoutMs);
}
