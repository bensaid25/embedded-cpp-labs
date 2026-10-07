#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include <ecl/hal/mock_sensor.hpp>

using ecl::SensorReading;
using ecl::hal::MockSensor;
using ecl::hal::MockSensorConfig;

TEST(MockSensor, WithoutNoiseReturnsTheBaselineExactly) {
    MockSensor s{MockSensorConfig{}.with_baseline(21.5f)};
    for (std::uint32_t t = 0; t < 5; ++t) {
        const auto r = s.read(t * 10);
        EXPECT_TRUE(r.is_valid());
        EXPECT_EQ(r.value(), 21.5f);
        EXPECT_EQ(r.timestamp_ms(), t * 10);
    }
    EXPECT_EQ(s.samples_taken(), 5u);
}

TEST(MockSensor, SameSeedGivesSameSequence) {
    const auto cfg = MockSensorConfig{}.with_noise(1.0f).with_seed(7);
    MockSensor a{cfg};
    MockSensor b{cfg};
    for (std::uint32_t i = 0; i < 100; ++i) {
        EXPECT_EQ(a.read(i), b.read(i));
    }
}

TEST(MockSensor, DifferentSeedsGiveDifferentSequences) {
    MockSensor a{MockSensorConfig{}.with_noise(1.0f).with_seed(1)};
    MockSensor b{MockSensorConfig{}.with_noise(1.0f).with_seed(2)};
    int differences = 0;
    for (std::uint32_t i = 0; i < 50; ++i) {
        if (a.read(i).value() != b.read(i).value()) {
            ++differences;
        }
    }
    EXPECT_GT(differences, 40);
}

TEST(MockSensor, NoiseStaysInsideItsAmplitude) {
    MockSensor s{
        MockSensorConfig{}.with_baseline(10.0f).with_noise(0.5f).with_seed(3)};
    float lowest = 100.0f;
    float highest = -100.0f;
    for (std::uint32_t i = 0; i < 10000; ++i) {
        const float v = s.read(i).value();
        lowest = std::fmin(lowest, v);
        highest = std::fmax(highest, v);
    }
    EXPECT_GE(lowest, 9.5f);
    EXPECT_LE(highest, 10.5f);
    // And the noise really uses the range, it is not stuck near zero.
    EXPECT_LT(lowest, 9.6f);
    EXPECT_GT(highest, 10.4f);
}

TEST(MockSensor, DriftGrowsWithTime) {
    MockSensor s{MockSensorConfig{}.with_baseline(20.0f).with_drift(0.5f)};
    EXPECT_FLOAT_EQ(s.read(0).value(), 20.0f);
    EXPECT_FLOAT_EQ(s.read(2000).value(), 21.0f);
    EXPECT_FLOAT_EQ(s.read(10000).value(), 25.0f);
}

TEST(MockSensor, SpikeHitsEveryNthSample) {
    MockSensor s{
        MockSensorConfig{}.with_baseline(10.0f).with_spikes(4, 50.0f)};
    for (std::uint32_t n = 1; n <= 12; ++n) {
        const float expected = (n % 4 == 0) ? 60.0f : 10.0f;
        EXPECT_EQ(s.read(n).value(), expected) << "sample " << n;
    }
}

TEST(MockSensor, FaultReturnsInvalidReadingWithTimestamp) {
    MockSensor s{MockSensorConfig{}.with_faults(3)};
    for (std::uint32_t n = 1; n <= 9; ++n) {
        const auto r = s.read(n * 100);
        EXPECT_EQ(r.is_valid(), n % 3 != 0) << "sample " << n;
        EXPECT_EQ(r.timestamp_ms(), n * 100);
    }
}

TEST(MockSensor, NaNBecomesAnInvalidReading) {
    MockSensor s{MockSensorConfig{}.with_nans(2)};
    EXPECT_TRUE(s.read(1).is_valid());
    const auto r = s.read(2);
    EXPECT_FALSE(r.is_valid());
    EXPECT_EQ(r.value(), 0.0f);  // SensorReading never leaks the NaN
    EXPECT_TRUE(s.read(3).is_valid());
}

TEST(MockSensor, FaultWinsOverSpike) {
    MockSensor s{MockSensorConfig{}.with_faults(2).with_spikes(2, 99.0f)};
    (void)s.read(1);
    EXPECT_FALSE(s.read(2).is_valid());
}

TEST(MockSensor, DefaultConstructedWorks) {
    MockSensor s;
    const auto r = s.read(5);
    EXPECT_TRUE(r.is_valid());
    EXPECT_EQ(r.value(), 20.0f);
}
