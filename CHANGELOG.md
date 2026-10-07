# Changelog

## 0.2.0

### Added
- `SpscQueue<T, N>`: lock-free single-producer, single-consumer queue for
  handing readings from an interrupt to the main loop. Never blocks, counts
  dropped readings, power-of-two capacity, checked with ThreadSanitizer.
- `ecl/hal`: the sensor contract (`is_sensor_v`), a deterministic
  `MockSensor` (noise, drift, spikes, failed reads, NaN) and the `Sampler`
  that feeds a queue.
- Host example `03_isr_pipeline`: a simulated timer interrupt feeding a main
  loop, including a main-loop stall and a drop-accounting check.
- Cortex-M4F cross-build (`cmake/toolchains/`), footprint programs
  (`footprint/`) and `scripts/footprint.sh`: flash/RAM report plus a check
  that the linked firmware contains no heap or exception support code.
- CI: ThreadSanitizer jobs, the cross-build with a footprint job summary, and
  an advisory clang-tidy job.
- `docs/architecture.md` and `docs/constraints.md`.
- CMake options `ECL_TSAN` and `ECL_BUILD_FOOTPRINT`.

### Changed
- README: new modules, interrupt example, design decisions, quality gates,
  footprint table and roadmap.
- `Architecture.txt` replaced by `docs/architecture.md`.

## 0.1.0

- `SensorReading`, `RingBuffer`, `Statistics`, `MovingAverage`,
  `ThresholdDetector`, host examples, CI with sanitizers.
