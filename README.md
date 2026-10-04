# embedded-cpp-labs

Hardware-independent, embedded-style C++17 building blocks for sensor data:
fixed-capacity containers, filters, statistics, and threshold detection.

## Constraints

- No dynamic allocation in the core library
- No exceptions or RTTI
- Header-only, tested on the host with GoogleTest

## Build and test

    cmake -S . -B build
    cmake --build build -j
    ctest --test-dir build --output-on-failure

## Status

| Module | Level | Status |
|---|---|---|
| SensorReading | Beginner | done |
| RingBuffer | Beginner | done |
| Statistics | Beginner | done |
| MovingAverage | Intermediate | done |
| ThresholdDetector | Intermediate | planned |
