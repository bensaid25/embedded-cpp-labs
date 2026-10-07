// The interrupt-to-main-loop pattern, simulated on the host:
//
//   "timer interrupt" every 10 ms:   MockSensor -> Sampler -> SpscQueue
//   main loop, every 50 ms:          SpscQueue -> MovingAverage -> ThresholdDetector
//                                                          \-> Statistics
//
// The simulation is single-threaded and fully deterministic, so the output
// is identical on every run and every machine. (tests/test_spsc_queue.cpp
// runs the real two-thread version under ThreadSanitizer.)
//
// What to look for in the output:
//   1. The main loop "freezes" from 1000 ms to 1400 ms (think: a long flash
//      write). The queue fills up and the interrupt side starts losing
//      readings instead of blocking. They are counted, not silently lost.
//   2. Failed reads travel through the queue as invalid readings and are
//      skipped by the filter and the statistics.
//   3. The temperature drifts upward, and the alert eventually trips for good.
//   4. Honest limitation: a 5-sample AVERAGE turns one +12 spike into +2.4.
//      Far from the limit that is harmless, but once the drift has brought the
//      baseline close, a lone spike can briefly push the average over it, so
//      you may see short HIGH episodes before the sustained one. A median
//      filter would ignore them; it is on the roadmap.
//   5. The final accounting adds up: produced = processed + dropped + queued.

#include <cstdint>
#include <cstdio>

#include <ecl/hal/mock_sensor.hpp>
#include <ecl/hal/sampler.hpp>
#include <ecl/moving_average.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/spsc_queue.hpp>
#include <ecl/statistics.hpp>
#include <ecl/threshold_detector.hpp>

namespace {

constexpr std::uint32_t kSamplePeriodMs = 10;   // the timer interrupt
constexpr std::uint32_t kMainLoopPeriodMs = 50; // how often the main loop wakes
constexpr std::uint32_t kStallStartMs = 1000;   // the main loop is busy...
constexpr std::uint32_t kStallEndMs = 1400;     // ...until here
constexpr std::uint32_t kDurationMs = 3000;
constexpr std::size_t kQueueCapacity = 16;      // must be a power of two

const char* name(ecl::ThresholdState state) {
    switch (state) {
        case ecl::ThresholdState::Normal: return "NORMAL";
        case ecl::ThresholdState::Low:    return "LOW";
        case ecl::ThresholdState::High:   return "HIGH";
    }
    return "?";
}

}  // namespace

int main() {
    // 20 degrees, +/-0.3 noise, a +12 spike on every 37th sample, a failed
    // read on every 23rd, and a slow drift of +2 degrees per second.
    ecl::hal::MockSensor sensor{ecl::hal::MockSensorConfig{}
                                    .with_baseline(20.0f)
                                    .with_noise(0.3f)
                                    .with_spikes(37, 12.0f)
                                    .with_faults(23)
                                    .with_drift(2.0f)};

    ecl::SpscQueue<ecl::SensorReading, kQueueCapacity> queue;
    ecl::hal::Sampler<ecl::hal::MockSensor, kQueueCapacity> sampler{sensor, queue};

    ecl::MovingAverage<5> filter;
    ecl::Statistics<> stats;
    auto detector = ecl::ThresholdDetector<>::make(10.0f, 25.0f, 1.0f);
    if (!detector) {
        std::puts("invalid detector configuration");
        return 1;
    }

    std::uint32_t processed = 0;
    std::uint32_t invalid_seen = 0;

    std::puts("  t(ms)  event");
    for (std::uint32_t t = 0; t < kDurationMs; t += kSamplePeriodMs) {
        // ---- interrupt side: runs on every timer tick, never blocks ----
        sampler.sample(t);

        // ---- main-loop side: wakes now and then, unless it is stalled ----
        const bool stalled = t >= kStallStartMs && t < kStallEndMs;
        if (stalled || t % kMainLoopPeriodMs != 0) {
            continue;
        }

        const std::size_t backlog = queue.size_approx();
        if (backlog >= kQueueCapacity) {
            std::printf("%7u  main loop is back: queue was full (%zu), "
                        "%zu readings lost so far\n",
                        static_cast<unsigned>(t), backlog, queue.dropped());
        }

        while (const auto reading = queue.try_pop()) {
            ++processed;
            if (!reading->is_valid()) {
                ++invalid_seen;
            }
            stats.add(*reading);
            filter.add(*reading);

            const auto average = filter.value();
            if (average && detector->update(*average)) {
                std::printf("%7u  state -> %-6s (average %.2f)\n",
                            static_cast<unsigned>(reading->timestamp_ms()),
                            name(detector->state()),
                            static_cast<double>(*average));
            }
        }
    }

    // Anything still queued at the end has been produced but not processed.
    const std::size_t left_in_queue = queue.size_approx();
    const std::uint32_t produced = sensor.samples_taken();

    std::printf("\nproduced by the interrupt side : %u\n", static_cast<unsigned>(produced));
    std::printf("processed by the main loop     : %u (%u of them failed reads, skipped)\n",
                static_cast<unsigned>(processed), static_cast<unsigned>(invalid_seen));
    std::printf("dropped (queue was full)       : %zu\n", queue.dropped());
    std::printf("still in the queue at the end  : %zu\n", left_in_queue);
    const bool balanced = produced == processed + queue.dropped() + left_in_queue;
    std::printf("accounting                     : %s\n", balanced ? "OK" : "MISMATCH");

    std::printf("\nvalid readings: %zu, mean %.2f\n", stats.count(),
                static_cast<double>(stats.mean().value()));
    return balanced ? 0 : 1;
}
