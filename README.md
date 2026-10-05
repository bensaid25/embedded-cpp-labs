# embedded-cpp-labs

![CI](https://github.com/bensaid25/embedded-cpp-labs/actions/workflows/ci.yml/badge.svg)

Embedded-style C++17 building blocks for sensor data: a fixed-capacity buffer, filters, statistics and threshold detection. Header-only, no heap allocation, no exceptions, and tested on the host so it can later run on a microcontroller unchanged.

## Why this exists

Firmware code that handles sensor data has to cope with limited memory, noisy values, failed readings and no room for surprises. This library is a set of small, independently tested pieces that follow those constraints from the start:

- **No dynamic allocation.** Capacities are template parameters, so memory use is known at compile time.
- **No exceptions and no RTTI.** Failure is reported with `bool` and `std::optional`.
- **Bad data is handled at the edge.** NaN, infinity and failed readings are rejected or skipped, and never spread through a pipeline.
- **Header-only, C++17.** Link one CMake target and include what you need.

## Modules

| Module | What it does | Key guarantees |
|---|---|---|
| `SensorReading` | One timestamped measurement with a validity flag | Trivially copyable. NaN and infinity become invalid readings. |
| `RingBuffer<T, N>` | Fixed-capacity circular FIFO | No heap, O(1) operations, overwrite-oldest or reject when full. |
| `Statistics` | Streaming count, min, max, mean and RMS | Constant memory, `double` accumulators, non-finite input rejected. |
| `MovingAverage<W>` | Sliding-window average | O(1) amortized, exact periodic resync so rounding error cannot build up. |
| `ThresholdDetector` | Low and high limits with hysteresis | Edge-triggered, no flicker near a limit, configuration validated by `make()`. |

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

## Design decisions

- **Capacity as a template parameter.** `RingBuffer<T, N>` and `MovingAverage<W>` use `std::array`, so there is no allocation and the footprint is `N * sizeof(T)` plus a few counters.
- **Two overflow policies, with clear names.** `push()` overwrites the oldest value, which suits streaming data. `try_push()` refuses and returns `false` when you cannot afford to lose data.
- **Validity travels with the data.** `SensorReading` is invalid if the caller says so or the value is not finite, and every other module skips invalid readings.
- **Wide accumulators and periodic resync.** Sums are kept in `double`. `MovingAverage` also recomputes its sum exactly every `W` samples, which is still O(1) amortized. A test with a `float` accumulator shows the drift this prevents.
- **Hysteresis, not a bare comparison.** Once a limit is crossed, the value must come back by a margin before the alert clears. On a noisy signal hovering at a limit, the plain detector changes state on every sample, and the one with hysteresis changes once (covered by a test).
- **Configuration is validated without exceptions.** `ThresholdDetector` has no public constructor. `make()` returns an empty `std::optional` for an invalid setup (low above high, negative hysteresis, non-finite limits).
- **One shared finite check.** `detail::is_finite` uses `x - x == 0`, which is `constexpr` in C++17. It assumes IEEE semantics, so it is not reliable under `-ffast-math`.

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
| `ECL_WARNINGS_AS_ERRORS` | `OFF` | Add `-Werror` to tests and examples |
| `ECL_SANITIZE` | `OFF` | Build tests and examples with AddressSanitizer and UBSan |

Use the library from your own CMake project:

```cmake
add_subdirectory(embedded-cpp-labs)
target_link_libraries(my_firmware PRIVATE ecl)
```

## Quality gates

Every push runs four CI jobs: gcc and clang, each in Release mode and with sanitizers, all with `-Wall -Wextra -Wpedantic -Werror`. Each module has tests for its boundary cases: empty and full containers, wrap-around, capacity of one, NaN and infinity, and numerical drift.

## Project layout

```
include/ecl/        public headers (one per module, plus detail/)
tests/              GoogleTest unit tests, one file per module
examples/host/      runnable examples, no hardware needed
.github/workflows/  CI
```

## Roadmap

- [x] Core modules: `SensorReading`, `RingBuffer`, `Statistics`, `MovingAverage`, `ThresholdDetector`
- [x] Host example and CI with sanitizers
- [ ] Run the same headers on a Raspberry Pi Pico or ESP32 with an MPU6050
- [ ] Measured flash, RAM and timing numbers on the target
- [ ] Lock-free single-producer, single-consumer buffer for ISR-to-main-loop use
- [ ] Fixed-point type for microcontrollers without an FPU
- [ ] STL-compatible iterators for `RingBuffer`
