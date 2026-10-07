# embedded-cpp-labs

![CI](https://github.com/bensaid25/embedded-cpp-labs/actions/workflows/ci.yml/badge.svg)

Embedded-style C++17 building blocks for sensor data: a fixed-capacity buffer, filters, statistics and threshold detection. Header-only, no heap allocation, no exceptions, and tested on the host so it can later run on a microcontroller unchanged.

## Why this exists

Firmware code that handles sensor data has to cope with limited memory, noisy values, failed readings and no room for surprises. This library is a set of small, independently tested pieces that follow those constraints from the start:

- **No dynamic allocation.** Capacities are template parameters, so memory use is known at compile time.
- **No exceptions and no RTTI.** Failure is reported with `bool` and `std::optional`.
- **Bad data is handled at the edge.** NaN, infinity and failed readings are rejected or skipped, and never spread through a pipeline.
- **Interrupt-safe handoff.** A lock-free queue moves readings from an interrupt to the main loop without ever blocking.
- **Header-only, C++17.** Link one CMake target and include what you need.

## Modules

| Module | What it does | Key guarantees |
|---|---|---|
| `SensorReading` | One timestamped measurement with a validity flag | Trivially copyable. NaN and infinity become invalid readings. |
| `RingBuffer<T, N>` | Fixed-capacity circular FIFO | No heap, O(1) operations, overwrite-oldest or reject when full. |
| `Statistics` | Streaming count, min, max, mean and RMS | Constant memory, `double` accumulators, non-finite input rejected. |
| `MovingAverage<W>` | Sliding-window average | O(1) amortized, exact periodic resync so rounding error cannot build up. |
| `ThresholdDetector` | Low and high limits with hysteresis | Edge-triggered, no flicker near a limit, configuration validated by `make()`. |
| `SpscQueue<T, N>` | Lock-free queue between one producer (an interrupt) and one consumer (the main loop) | Never blocks, no heap, counts readings it had to drop. Checked with ThreadSanitizer. |
| `hal::Sensor` contract | What a sensor driver must provide: `read(now_ms)` | Compile-time check, no virtual functions. |
| `hal::MockSensor` | Simulated sensor with noise, drift, spikes, failed reads and NaN | Deterministic: same seed, same readings, on every machine. |
| `hal::Sampler` | Reads a sensor into an `SpscQueue` | The interrupt-side half of a pipeline. Never blocks. |

## Quick start

```cpp
#include <ecl/moving_average.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/threshold_detector.hpp>

ecl::MovingAverage<8> smoother;
auto detector = ecl::ThresholdDetector<>::make(10.0f, 25.0f, 1.0f);  // low, high, hysteresis

void on_sample(std::uint32_t t_ms, float raw) {
    const ecl::SensorReading reading{raw, t_ms};  // NaN or infinity become invalid
    smoother.add(reading);                        // invalid readings are skipped

    if (const auto avg = smoother.value(); avg && detector) {
        if (detector->update(*avg)) {
            // The state changed: raise or clear an alert here.
        }
    }
}
```

### From an interrupt to the main loop

```cpp
#include <ecl/hal/mock_sensor.hpp>
#include <ecl/hal/sampler.hpp>

ecl::hal::MockSensor sensor{ecl::hal::MockSensorConfig{}.with_baseline(20.0f).with_noise(0.3f)};
ecl::SpscQueue<ecl::SensorReading, 16> queue;           // capacity: a power of two
ecl::hal::Sampler<ecl::hal::MockSensor, 16> sampler{sensor, queue};

void timer_interrupt(std::uint32_t now_ms) {
    sampler.sample(now_ms);                             // never blocks
}

void main_loop_iteration() {
    while (const auto reading = queue.try_pop()) {
        smoother.add(*reading);  // the reading travels intact: failed reads stay invalid
        if (const auto avg = smoother.value(); avg && detector && detector->update(*avg)) {
            // The state changed: raise or clear an alert here.
        }
    }
}
```

(`smoother` and `detector` are the ones from the quick start above.) On real hardware, replace `MockSensor` with a class that has the same `read(now_ms)` function (an I2C accelerometer, an ADC channel). Nothing else changes.

## Example: a full sensor pipeline

`examples/host/01_sensor_pipeline.cpp` chains every module on a simulated temperature sensor: 20 degrees with noise, one failed reading at 700 ms, and a hot spell of 30 degrees from 1500 ms to 2400 ms.

```
SensorReading -> MovingAverage -> ThresholdDetector (alert above 25, clear below 24)
              -> Statistics (summary of the raw valid readings)
```

```bash
./build/examples/host/sensor_pipeline
```

Excerpt of the output:

```
  t(ms)     raw     avg  state
    700   fault   19.73  NORMAL
   1500   30.15   22.30  NORMAL
   1600   29.35   24.06  NORMAL
   1700   29.42   25.95  HIGH    <-- state changed
   2600   20.51   26.08  HIGH
   2700   19.45   24.07  HIGH
   2800   20.53   22.16  NORMAL  <-- state changed

raw readings: 39 valid
  min  19.24
  max  30.72
  mean 22.61
  rms  23.03
```

What it shows:

- **Smoothing has a cost.** The raw value jumps at 1500 ms, but the alert fires at 1700 ms, once the 5-sample average crosses 25.
- **Hysteresis works.** At 2700 ms the average (24.07) is already below the limit, yet the alert holds until it drops below 24.
- **A failed reading is harmless.** At 700 ms the filter keeps its previous average, and the reading is not counted in the statistics.

## Example: an interrupt feeding a main loop

`examples/host/03_isr_pipeline.cpp` simulates a 10 ms timer interrupt that samples a `MockSensor` (noise, spikes, failed reads, upward drift) into an `SpscQueue`, while a main loop wakes every 50 ms to run the pipeline. From 1000 ms to 1400 ms the main loop is "frozen", so the queue fills up and the interrupt side starts dropping readings instead of blocking. The final accounting shows nothing is lost silently:

```
produced by the interrupt side : 300
processed by the main loop     : 267 (10 of them failed reads, skipped)
dropped (queue was full)       : 29
still in the queue at the end  : 4
accounting                     : OK
```

The simulation is single-threaded and deterministic. The real two-thread version runs in `tests/test_spsc_queue.cpp` under ThreadSanitizer.

## Design decisions

- **Capacity as a template parameter.** `RingBuffer<T, N>` and `MovingAverage<W>` use `std::array`, so there is no allocation and the footprint is `N * sizeof(T)` plus a few counters.
- **Two overflow policies, with clear names.** `push()` overwrites the oldest value, which suits streaming data. `try_push()` refuses and returns `false` when you cannot afford to lose data.
- **Validity travels with the data.** `SensorReading` is invalid if the caller says so or the value is not finite, and every other module skips invalid readings.
- **Wide accumulators and periodic resync.** Sums are kept in `double`. `MovingAverage` also recomputes its sum exactly every `W` samples, which is still O(1) amortized. A test with a `float` accumulator shows the drift this prevents.
- **Hysteresis, not a bare comparison.** Once a limit is crossed, the value must come back by a margin before the alert clears. On a noisy signal hovering at a limit, the plain detector changes state on every sample, and the one with hysteresis changes once (covered by a test).
- **Configuration is validated without exceptions.** `ThresholdDetector` has no public constructor. `make()` returns an empty `std::optional` for an invalid setup (low above high, negative hysteresis, non-finite limits).
- **A queue that drops instead of blocking.** `SpscQueue::try_push` returns `false` and counts the loss when the queue is full. In an interrupt, losing one sample is better than waiting, and the `dropped()` counter makes the loss visible.
- **Lock-free with the weakest correct ordering.** The producer publishes a slot with a release store and the consumer reads it with an acquire load. Each index has exactly one writer, so no atomic read-modify-write is needed, which also keeps it working on cores without them. The reasoning is written in the header.
- **Power-of-two capacity.** Indices wrap with a bit mask instead of a division, because many small cores have no hardware divider. A wrong capacity is a compile error.
- **Sensors are a compile-time contract, not a base class.** Any type with `read(now_ms)` works, so the call can be inlined and no vtable is stored in flash. The caller owns the clock, so a driver has no hidden time dependency.
- **A deterministic mock sensor.** Tests and demos inject spikes, failed reads and NaN on a schedule, so every edge case is reproducible.
- **One shared finite check.** `detail::is_finite` compares against `numeric_limits<T>::max()`, which is `constexpr` in C++17 and fails for NaN and both infinities. Like any NaN test, it is not reliable under `-ffast-math`.
- **Not thread-safe.** None of the containers or filters may be shared between an interrupt handler and the main code without external protection. A lock-free single-producer, single-consumer buffer for that case is on the roadmap.

## Build and test

Requires CMake 3.20 or newer and a C++17 compiler. GoogleTest is downloaded automatically.

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

| CMake option | Default | Effect |
|---|---|---|
| `ECL_BUILD_TESTS` | `ON` | Build the unit tests |
| `ECL_BUILD_EXAMPLES` | `ON` | Build the host examples |
| `ECL_BUILD_FOOTPRINT` | `OFF` | Build the flash/RAM measurement programs (for cross-compiling) |
| `ECL_WARNINGS_AS_ERRORS` | `OFF` | Add `-Werror` to tests and examples |
| `ECL_SANITIZE` | `OFF` | Build tests and examples with AddressSanitizer and UBSan |
| `ECL_TSAN` | `OFF` | Build tests and examples with ThreadSanitizer (cannot be combined with `ECL_SANITIZE`) |

Use the library from your own CMake project:

```cmake
add_subdirectory(embedded-cpp-labs)
target_link_libraries(my_firmware PRIVATE ecl)
```

## Quality gates

Every push runs:

- **Six test jobs:** gcc and clang, each in Release, with AddressSanitizer + UBSan, and with ThreadSanitizer, all with `-Wall -Wextra -Wpedantic -Werror`. Each module has tests for its boundary cases: empty and full containers, wrap-around, capacity of one, NaN and infinity, numerical drift, and (for the queue) a two-thread stress test.
- **A Cortex-M4 cross-build** with `arm-none-eabi-g++ -fno-exceptions -fno-rtti`. A `throw` anywhere in the library would be a compile error, the linked program is scanned to prove it contains no `malloc` and no `operator new`, and flash and RAM are measured.
- **clang-tidy** (advisory for now).

## Footprint on a Cortex-M4F

Measured by `scripts/footprint.sh` on firmware-shaped programs (`footprint/`), built with `arm-none-eabi-g++ 13.2.1`, `-Os`, newlib-nano. Each row is the cost of one module over an empty program. RAM counts static variables only (not stack or heap).

| Module | Flash | RAM |
|---|---:|---:|
| `RingBuffer<SensorReading, 64>` | +156 B | +776 B |
| `SpscQueue<SensorReading, 64>` | +212 B | +780 B |
| `MockSensor` + `Sampler` + `SpscQueue` | +388 B | +816 B |

RAM is as expected: 64 readings of 12 bytes, plus a few counters. The full table for every module is in the job summary of each CI run, and the same script works on your own build:

```bash
cmake -S . -B build-arm \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-cortex-m4.cmake \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DECL_BUILD_TESTS=OFF -DECL_BUILD_EXAMPLES=OFF -DECL_BUILD_FOOTPRINT=ON
cmake --build build-arm
scripts/footprint.sh build-arm/footprint arm-none-eabi-
```

See [docs/architecture.md](docs/architecture.md) for how the modules fit together and [docs/constraints.md](docs/constraints.md) for the rules and what is not promised.

## Project layout

```
include/ecl/        public headers (one per module, plus hal/ and detail/)
tests/              GoogleTest unit tests, one file per module
examples/host/      runnable examples, no hardware needed
footprint/          firmware-shaped programs used to measure flash and RAM
cmake/toolchains/   arm-none-eabi toolchain file for the cross-build
scripts/            footprint report and the heap/exception check
docs/               architecture and constraints
.github/workflows/  CI
```

## Roadmap

- [x] Core modules: `SensorReading`, `RingBuffer`, `Statistics`, `MovingAverage`, `ThresholdDetector`
- [x] Host example and CI with sanitizers
- [ ] Run the same headers on a Raspberry Pi Pico or ESP32 with an MPU6050
- [ ] Measured flash, RAM and timing numbers on the target
- [x] Lock-free single-producer, single-consumer queue for ISR-to-main-loop use
- [x] Sensor contract, mock sensor and sampler (host today, real driver later)
- [x] Cortex-M4 cross-compile in CI, with flash/RAM report and a no-heap check
- [ ] Median filter (ignores single spikes, which an average cannot)
- [ ] Fixed-point type for microcontrollers without an FPU
- [ ] STL-compatible iterators for `RingBuffer`
