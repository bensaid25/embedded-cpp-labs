#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#include <ecl/moving_average.hpp>
#include <ecl/sensor_reading.hpp>

using ecl::MovingAverage;
using ecl::SensorReading;

static_assert(MovingAverage<8>::window() == 8);

TEST(MovingAverage, NewFilterIsEmpty) {
    MovingAverage<4> ma;
    EXPECT_TRUE(ma.empty());
    EXPECT_FALSE(ma.full());
    EXPECT_EQ(ma.count(), 0u);
    EXPECT_EQ(ma.window(), 4u);
    EXPECT_EQ(ma.value(), std::nullopt);
}

TEST(MovingAverage, SingleSampleIsItsOwnAverage) {
    MovingAverage<4> ma;
    EXPECT_TRUE(ma.add(7.5f));
    EXPECT_FLOAT_EQ(ma.value().value(), 7.5f);
    EXPECT_EQ(ma.count(), 1u);
}

TEST(MovingAverage, WarmUpAveragesTheSamplesReceivedSoFar) {
    MovingAverage<4> ma;
    ma.add(2.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), 2.0f);
    ma.add(4.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), 3.0f);
    ma.add(6.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), 4.0f);
    EXPECT_FALSE(ma.full());
    ma.add(8.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), 5.0f);
    EXPECT_TRUE(ma.full());
}

TEST(MovingAverage, WindowSlidesAndDropsTheOldestSample) {
    MovingAverage<3> ma;
    ma.add(1.0f);
    ma.add(2.0f);
    ma.add(3.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), 2.0f);  // 1, 2, 3
    ma.add(4.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), 3.0f);  // 2, 3, 4
    ma.add(5.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), 4.0f);  // 3, 4, 5
    EXPECT_EQ(ma.count(), 3u);
}

TEST(MovingAverage, WindowOfOneReturnsTheLatestSample) {
    MovingAverage<1> ma;
    ma.add(10.0f);
    ma.add(-4.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), -4.0f);
    EXPECT_EQ(ma.count(), 1u);
}

TEST(MovingAverage, ConstantInputStaysConstant) {
    MovingAverage<5> ma;
    for (int i = 0; i < 100; ++i) {
        ma.add(3.25f);
        EXPECT_FLOAT_EQ(ma.value().value(), 3.25f);
    }
}

TEST(MovingAverage, StepResponseRampsUpOverTheWindow) {
    MovingAverage<4> ma;
    for (int i = 0; i < 4; ++i) {
        ma.add(0.0f);
    }
    const float expected[] = {2.5f, 5.0f, 7.5f, 10.0f};
    for (float e : expected) {
        ma.add(10.0f);
        EXPECT_FLOAT_EQ(ma.value().value(), e);
    }
}

TEST(MovingAverage, SmoothsAlternatingNoise) {
    MovingAverage<4> ma;
    for (int i = 0; i < 20; ++i) {
        ma.add(i % 2 == 0 ? 19.0f : 21.0f);
    }
    EXPECT_NEAR(ma.value().value(), 20.0f, 1e-5f);
}

TEST(MovingAverage, RejectsNaNAndInfinity) {
    MovingAverage<3> ma;
    ma.add(2.0f);
    ma.add(4.0f);
    EXPECT_FALSE(ma.add(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(ma.add(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(ma.add(-std::numeric_limits<float>::infinity()));
    EXPECT_EQ(ma.count(), 2u);
    EXPECT_FLOAT_EQ(ma.value().value(), 3.0f);
}

TEST(MovingAverage, OnlyRejectedSamplesLeavesItEmpty) {
    MovingAverage<3> ma;
    EXPECT_FALSE(ma.add(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_TRUE(ma.empty());
    EXPECT_EQ(ma.value(), std::nullopt);
}

TEST(MovingAverage, IgnoresInvalidSensorReadings) {
    MovingAverage<3> ma;
    EXPECT_TRUE(ma.add(SensorReading{10.0f, 1}));
    EXPECT_FALSE(ma.add(SensorReading::invalid(2)));
    EXPECT_TRUE(ma.add(SensorReading{20.0f, 3}));
    EXPECT_EQ(ma.count(), 2u);
    EXPECT_FLOAT_EQ(ma.value().value(), 15.0f);
}

TEST(MovingAverage, ResetForgetsEverything) {
    MovingAverage<3> ma;
    ma.add(100.0f);
    ma.add(200.0f);
    ma.reset();
    EXPECT_TRUE(ma.empty());
    EXPECT_EQ(ma.value(), std::nullopt);
    ma.add(4.0f);
    EXPECT_FLOAT_EQ(ma.value().value(), 4.0f);
    EXPECT_EQ(ma.count(), 1u);
}

TEST(MovingAverage, MatchesABruteForceAverageOverALongRun) {
    constexpr std::size_t kWindow = 7;
    MovingAverage<kWindow> ma;
    float history[1000];
    std::uint32_t state = 12345;
    for (std::size_t n = 0; n < 1000; ++n) {
        state = state * 1664525u + 1013904223u;  // simple deterministic noise
        history[n] = static_cast<float>(state >> 16) / 655.35f - 50.0f;
        ma.add(history[n]);

        const std::size_t count = n + 1 < kWindow ? n + 1 : kWindow;
        double sum = 0.0;
        for (std::size_t k = 0; k < count; ++k) {
            sum += static_cast<double>(history[n - k]);
        }
        EXPECT_NEAR(ma.value().value(), sum / static_cast<double>(count), 1e-3)
            << "at sample " << n;
    }
}

TEST(MovingAverage, PeriodicResyncRemovesARoundingErrorInAFloatAccumulator) {
    // A float accumulator loses the small values next to the huge one, so
    // the running sum is wrong once the huge value leaves the window.
    // The exact recomputation every `Window` samples repairs it.
    MovingAverage<4, float, float> ma;
    ma.add(1e8f);
    for (int i = 0; i < 20; ++i) {
        ma.add(1.0f);
    }
    EXPECT_FLOAT_EQ(ma.value().value(), 1.0f);
}

TEST(MovingAverage, DoubleValueTypeWorks) {
    MovingAverage<2, double> ma;
    ma.add(0.5);
    ma.add(1.5);
    EXPECT_DOUBLE_EQ(ma.value().value(), 1.0);
}
