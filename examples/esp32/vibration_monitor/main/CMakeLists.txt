idf_component_register(
    SRCS "main.cpp" "mpu6050.cpp"
    INCLUDE_DIRS "."
    REQUIRES ecl esp_driver_i2c esp_timer
)
