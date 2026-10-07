# Embedded C++ guidelines: how this library follows them

Common rules for firmware C++, with what this repository does about each one, what enforces it, and what is still open. "Enforced" means a tool in CI fails the build when the rule is broken.

| # | Rule | Status | Evidence |
|---|---|---|---|
| 1 | No unbounded recursion | Enforced | The library has no recursion. `clang-tidy` runs `misc-no-recursion` in CI. |
| 2 | Bounded loops, timeouts | Partly | The only loop in the library (`MovingAverage::resync`) runs at most `Window` times. In the ESP32 example every I2C transfer has a 100 ms timeout, and a failed read becomes an invalid reading. Open: a task watchdog. |
| 3 | Explicit types and sizes | Enforced, with one trade-off | Sizes use `std::size_t`, timestamps `std::uint32_t`, values `float`. CI builds with `-Wconversion -Wsign-conversion -Wdouble-promotion -Wshadow -Werror`. Trade-off: the default accumulators are `double`, which can be slow on chips without a double-precision FPU. The `Acc` template parameter lets you use `float`. To be measured on the target. |
| 4 | Compile-time checks | Enforced | `static_assert` on capacities, windows and types. Capacities are template parameters. `SensorReading` and `ThresholdDetector` work in `constexpr` code, and a test checks it at compile time. |
| 5 | Few virtual functions | Enforced by construction | No virtual functions anywhere. Polymorphism is by templates. |
| 6 | Interrupts and concurrency | Open | The containers and filters are not safe to share between an interrupt handler and the main code. A lock-free single-producer, single-consumer buffer is on the roadmap. See the note on `volatile` below. |
| 7 | No iostream, no heap containers | Enforced | The library includes only `<array>`, `<optional>`, `<cstddef>`, `<cstdint>`, `<limits>`, `<cmath>`, `<type_traits>` and `<cassert>`. A compile-only check builds every header, with every template instantiated, under `-fno-exceptions -fno-rtti`. Examples use `printf`. |
| 8 | Quality tools | Mostly | Warnings as errors, AddressSanitizer and UBSan, and `clang-tidy` (config in `.clang-tidy`) run on every push. Not adopted: a formal coding standard. |

## Notes

**`volatile` is not a synchronization tool.** It stops the compiler from optimizing away accesses, which is what memory-mapped hardware registers need. It gives no atomicity and no ordering between an interrupt handler and the main code. For shared data use `std::atomic` with explicit memory ordering, or a short critical section (interrupts disabled).

**Coding standards.** MISRA C++:2023 is the current MISRA standard for C++17. Claiming compliance needs a dedicated checker and documented deviations, so this project follows selected principles (no recursion, no dynamic allocation, no exceptions, explicit conversions) without claiming compliance.

**Why `std::optional` and not exceptions.** Failure is reported by return value. `std::optional::value()` can throw, so the library and the firmware example use `value_or()` or check before dereferencing. `clang-tidy` (`bugprone-unchecked-optional-access`) flags unchecked access.

**Known API weakness.** `SensorReading(value, timestamp, valid)` and `ThresholdDetector::make(low, high, hysteresis)` take adjacent parameters of similar types that are easy to swap by mistake. `clang-tidy` reports this (`bugprone-easily-swappable-parameters`), and the check is currently switched off. A named-fields configuration struct or strong types would fix it.
