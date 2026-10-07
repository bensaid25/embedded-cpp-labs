#include <gtest/gtest.h>

#include <cstdint>

#include <ecl/hal/mock_sensor.hpp>
#include <ecl/hal/sampler.hpp>
#include <ecl/hal/sensor.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/spsc_queue.hpp>

using ecl::SensorReading;
using ecl::SpscQueue;
using ecl::hal::MockSensor;
using ecl::hal::MockSensorConfig;
using ecl::hal::Sampler;

namespace {

// A hand-written sensor: shows that any type with the right read() works,
// with no base class to inherit from.
struct CountingSensor {
    SensorReading read(SensorReading::Timestamp now_ms) noexcept {
        return SensorReading{static_cast<float>(++calls), now_ms};
    }
    int calls = 0;
};

struct NotASensor {};
struct WrongSignature {
    float read(int) { return 0.0f; }
};

}  // namespace

static_assert(ecl::hal::is_sensor_v<MockSensor>);
static_assert(ecl::hal::is_sensor_v<CountingSensor>);
static_assert(!ecl::hal::is_sensor_v<NotASensor>);
static_assert(!ecl::hal::is_sensor_v<WrongSignature>);
static_assert(!ecl::hal::is_sensor_v<int>);

TEST(Sampler, PushesEachReadingIntoTheQueue) {
    CountingSensor sensor;
    SpscQueue<SensorReading, 4> queue;
    Sampler<CountingSensor, 4> sampler{sensor, queue};

    EXPECT_TRUE(sampler.sample(10));
    EXPECT_TRUE(sampler.sample(20));

    EXPECT_EQ(*queue.try_pop(), (SensorReading{1.0f, 10}));
    EXPECT_EQ(*queue.try_pop(), (SensorReading{2.0f, 20}));
    EXPECT_FALSE(queue.try_pop().has_value());
}

TEST(Sampler, ReportsAndCountsLostReadingsWhenTheQueueIsFull) {
    CountingSensor sensor;
    SpscQueue<SensorReading, 2> queue;
    Sampler<CountingSensor, 2> sampler{sensor, queue};

    EXPECT_TRUE(sampler.sample(1));
    EXPECT_TRUE(sampler.sample(2));
    EXPECT_FALSE(sampler.sample(3));
    EXPECT_FALSE(sampler.sample(4));

    EXPECT_EQ(queue.dropped(), 2u);
    EXPECT_EQ(sensor.calls, 4);  // the sensor was still read each time
    EXPECT_EQ(queue.try_pop()->timestamp_ms(), 1u);
    EXPECT_EQ(queue.try_pop()->timestamp_ms(), 2u);
}

TEST(Sampler, FailedReadingsTravelThroughAsInvalid) {
    MockSensor sensor{MockSensorConfig{}.with_faults(2)};
    SpscQueue<SensorReading, 4> queue;
    Sampler<MockSensor, 4> sampler{sensor, queue};

    EXPECT_TRUE(sampler.sample(100));
    EXPECT_TRUE(sampler.sample(200));

    EXPECT_TRUE(queue.try_pop()->is_valid());
    const auto second = queue.try_pop();
    ASSERT_TRUE(second.has_value());
    EXPECT_FALSE(second->is_valid());
    EXPECT_EQ(second->timestamp_ms(), 200u);
}
