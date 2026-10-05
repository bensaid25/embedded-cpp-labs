# ESP32 vibration monitor

Runs the `ecl` library on an ESP32 with an MPU6050 accelerometer. The sensor is sampled at 200 Hz, gravity is removed, and once per second the firmware prints the vibration RMS and peak, the alert state, and how long the processing took.

The signal chain is the same header that the host unit tests and `examples/host/02_vibration_demo.cpp` use (`examples/common/vibration_pipeline.hpp`):

```
MPU6050 -> SensorReading -> MovingAverage (removes gravity) -> Statistics (RMS, peak per window)
        -> RingBuffer (recent RMS history) -> ThresholdDetector (alert with hysteresis)
```

## Wiring (ESP32 DevKit)

| MPU6050 | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| AD0 | GND (I2C address 0x68) |

Other boards use other default pins: change `kSdaPin` and `kSclPin` at the top of `main/main.cpp`.

## Build and flash

Open an ESP-IDF terminal (installed with ESP-IDF), then from this folder:

```
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with your board's port (see Device Manager on Windows) and use your chip name in `set-target` if it is not a classic ESP32 (for example `esp32s3`). Leave the serial monitor with `Ctrl+]`.

## What you should see

One line per second. Lying still, the RMS is a few thousandths of a g. Shaking the board raises it, and above 0.05 g RMS the state changes to `ALERT` until the RMS falls below 0.03 g.

The `read_us` and `proc_us` columns give the average and the maximum time per sample spent reading the sensor over I2C and running the ecl pipeline. Footprint numbers come from `idf.py size`.

## Limits of this demo

- 200 Hz sampling shows the pipeline working, but it is far too slow for real bearing fault analysis, which needs several kilohertz. The MPU6050 over I2C also limits the practical rate.
- It measures the magnitude of the acceleration vector, which keeps the demo simple and independent of the board's orientation.
