// One source file, one scenario per build (selected with -DFP_<NAME>).
//
// Every scenario defines step(t), which exercises one part of the library on
// values the compiler cannot predict (they come from volatile variables), so
// nothing is optimized away. The programs are never meant to run: they exist
// to be linked and measured. The BASELINE scenario does nothing; the cost of
// a module is its size minus the baseline.

#include <cstdint>

#include <ecl/hal/mock_sensor.hpp>
#include <ecl/hal/sampler.hpp>
#include <ecl/moving_average.hpp>
#include <ecl/ring_buffer.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/spsc_queue.hpp>
#include <ecl/statistics.hpp>
#include <ecl/threshold_detector.hpp>

namespace {

volatile float g_input = 21.0f;
volatile float g_output = 0.0f;

#if defined(FP_BASELINE)

void step(std::uint32_t t) {
    (void)t;
    g_output = g_input;
}

#elif defined(FP_RING_BUFFER)

ecl::RingBuffer<ecl::SensorReading, 64> g_buffer;

void step(std::uint32_t t) {
    g_buffer.push(ecl::SensorReading{g_input, t});
    if (const auto r = g_buffer.pop()) {
        g_output = r->value();
    }
}

#elif defined(FP_SPSC)

ecl::SpscQueue<ecl::SensorReading, 64> g_queue;

void step(std::uint32_t t) {
    (void)g_queue.try_push(ecl::SensorReading{g_input, t});
    if (const auto r = g_queue.try_pop()) {
        g_output = r->value();
    }
    g_output = g_output + static_cast<float>(g_queue.dropped());
}

#elif defined(FP_SAMPLER_MOCK)

ecl::hal::MockSensor g_sensor{ecl::hal::MockSensorConfig{}
                                  .with_baseline(20.0f)
                                  .with_noise(0.5f)
                                  .with_spikes(50, 8.0f)
                                  .with_faults(30)};
ecl::SpscQueue<ecl::SensorReading, 64> g_queue;
ecl::hal::Sampler<ecl::hal::MockSensor, 64> g_sampler{g_sensor, g_queue};

void step(std::uint32_t t) {
    (void)g_sampler.sample(t);  // the "interrupt" side
    if (const auto r = g_queue.try_pop()) {  // the "main loop" side
        g_output = r->value();
    }
}

#elif defined(FP_MOVING_AVERAGE)

ecl::MovingAverage<8> g_filter;

void step(std::uint32_t t) {
    g_filter.add(ecl::SensorReading{g_input, t});
    if (const auto avg = g_filter.value()) {
        g_output = *avg;
    }
}

#elif defined(FP_STATISTICS)

ecl::Statistics<> g_stats;

void step(std::uint32_t t) {
    g_stats.add(ecl::SensorReading{g_input, t});
    if (const auto mean = g_stats.mean()) {
        g_output = static_cast<float>(*mean);
    }
}

#elif defined(FP_THRESHOLD)

auto g_detector = ecl::ThresholdDetector<>::make(g_input - 10.0f, g_input + 4.0f, 1.0f);

void step(std::uint32_t t) {
    (void)t;
    if (g_detector && g_detector->update(g_input)) {
        g_output = g_input;
    }
}

#elif defined(FP_FULL)

// The whole chain: sensor -> sampler -> queue -> filter -> detector + stats.
ecl::hal::MockSensor g_sensor{ecl::hal::MockSensorConfig{}
                                  .with_baseline(20.0f)
                                  .with_noise(0.5f)
                                  .with_spikes(50, 8.0f)
                                  .with_faults(30)};
ecl::SpscQueue<ecl::SensorReading, 64> g_queue;
ecl::hal::Sampler<ecl::hal::MockSensor, 64> g_sampler{g_sensor, g_queue};
ecl::MovingAverage<8> g_filter;
ecl::Statistics<> g_stats;
auto g_detector = ecl::ThresholdDetector<>::make(10.0f, 25.0f, 1.0f);

void step(std::uint32_t t) {
    (void)g_sampler.sample(t);
    while (const auto r = g_queue.try_pop()) {
        g_stats.add(*r);
        g_filter.add(*r);
        if (const auto avg = g_filter.value()) {
            if (g_detector && g_detector->update(*avg) &&
                g_detector->state() == ecl::ThresholdState::High) {
                g_output = *avg;
            }
        }
    }
    if (const auto mean = g_stats.mean()) {
        g_output = g_output + static_cast<float>(*mean);
    }
}

#else
#error "Define one scenario: FP_BASELINE, FP_RING_BUFFER, FP_SPSC, ..."
#endif

}  // namespace

int main() {
    for (std::uint32_t t = 0;; t += 5) {
        step(t);
    }
}
