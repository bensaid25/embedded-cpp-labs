# Constraints

Every module in `ecl` follows the same rules, so the library can run on a
small microcontroller. Each rule has a reason, and where possible a check.

| Rule | Why | Checked by |
|---|---|---|
| **No heap allocation.** Capacities are template parameters; storage is `std::array`. | Memory use is known at compile time. No fragmentation, no out-of-memory at run time. | The linked firmware is scanned for `malloc` and `operator new` |
| **No exceptions.** Failure is a `bool`, a `std::optional`, or an invalid `SensorReading`. | Exception support costs flash and RAM, and unwinding has no bounded run time. | Cross-build with `-fno-exceptions` |
| **No RTTI, no virtual functions.** Polymorphism is by template. | No vtables in flash, and calls can be inlined. | Cross-build with `-fno-rtti` |
| **Bad data is stopped at the edge.** NaN, infinity and failed reads become invalid readings and are skipped. | One bad sample must not poison an average for the rest of the run. | Unit tests per module |
| **Interrupt-safe means non-blocking.** `SpscQueue::try_push` and `try_pop` never wait, loop or allocate. | An interrupt handler that waits can deadlock the system. | ThreadSanitizer stress tests |
| **Header-only, C++17.** | One CMake target, no build step for users. | CI on gcc and clang |

## Things the library does not promise

- **`-ffast-math` is not supported.** `detail::is_finite` relies on IEEE
  semantics.
- **`SpscQueue` is correct only for one producer and one consumer.** Nothing
  checks this at run time; using two producers is a bug in the caller.
- **`SpscQueue` needs a lock-free `std::atomic<std::size_t>`.** A
  `static_assert` fails the build on targets without one instead of silently
  using a lock. Checked with `arm-none-eabi-g++`: Cortex-M3, M4 and M7 are
  fine; Cortex-M0 (no exclusive-access instructions) is rejected at compile
  time.
- **Thread sanitizers do not prove a memory ordering correct.** x86 is
  strongly ordered, so a too-weak ordering often passes there anyway. The
  ordering in `spsc_queue.hpp` is documented and argued in the header, and the
  tests catch real races, but only a run on weakly ordered hardware (an ARM
  core) is final proof.
