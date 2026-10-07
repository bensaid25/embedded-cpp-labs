#pragma once

#include <cstddef>

#include <ecl/hal/sensor.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/spsc_queue.hpp>

namespace ecl::hal {

/// The producer half of a sensor pipeline: reads a sensor and pushes the
/// reading into an SpscQueue. Call sample() from a timer interrupt (or a
/// sampling thread). The main loop drains the queue with try_pop().
///
///     timer ISR:  sampler.sample(now_ms)         // producer side
///     main loop:  while (auto r = queue.try_pop()) { ...pipeline... }
///
/// sample() never blocks and never allocates. If the queue is full the
/// reading is lost and counted in queue.dropped(): in an interrupt it is
/// better to lose one sample than to wait.
///
/// The Sampler holds references, so the sensor and the queue must outlive it.
template <typename Sensor, std::size_t QueueCapacity>
class Sampler {
    static_assert(is_sensor_v<Sensor>,
                  "Sensor must provide: SensorReading read(Timestamp) noexcept");

public:
    using Queue = SpscQueue<SensorReading, QueueCapacity>;

    constexpr Sampler(Sensor& sensor, Queue& queue) noexcept
        : sensor_{sensor}, queue_{queue} {}

    /// Takes one reading. Returns false if the queue was full (reading lost).
    bool sample(SensorReading::Timestamp now_ms) noexcept {
        return queue_.try_push(sensor_.read(now_ms));
    }

private:
    Sensor& sensor_;
    Queue& queue_;
};

}  // namespace ecl::hal
