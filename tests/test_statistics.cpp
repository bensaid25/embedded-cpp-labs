#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <ecl/ring_buffer.hpp>
#include <ecl/sensor_reading.hpp>
#include <ecl/statistics.hpp>

using ecl::RingBuffer;
using ecl::SensorReading;
using ecl::Statistics;

TEST(Statistics, NewAccumulatorIsEmpty) {
    Statistics<> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.count(), 0u);
    EXPECT_EQ(s.min(), std::nullopt);
    EXPECT_EQ(s.max(), std::nullopt);
    EXPECT_EQ(s.mean(), std::nullopt);
    EXPECT_EQ(s.rms(), std::nullopt);
}

TEST(Statistics, SingleSample) {
    Statistics<> s;
    EXPECT_TRUE(s.add(-3.0f));
    EXPECT_EQ(s.count(), 1u);
    EXPECT_FLOAT_EQ(s.min().value(), -3.0f);
    EXPECT_FLOAT_EQ(s.max().value(), -3.0f);
    EXPECT_FLOAT_EQ(s.mean().value(), -3.0f);
    EXPECT_FLOAT_EQ(s.rms().value(), 3.0f);  // RMS is never negative
}

TEST(Statistics, KnownValues) {
    Statistics<> s;
    for (float x : {1.0f, 2.0f, 3.0f, 4.0f}) {
        s.add(x);
    }
    EXPECT_FLOAT_EQ(s.min().value(), 1.0f);
    EXPECT_FLOAT_EQ(s.max().value(), 4.0f);
    EXPECT_FLOAT_EQ(s.mean().value(), 2.5f);
    EXPECT_NEAR(s.rms().value(), std::sqrt(7.5), 1e-5);  // sqrt(30 / 4)
}

TEST(Statistics, NegativeValues) {
    Statistics<> s;
    for (float x : {-3.0f, -1.0f, -2.0f}) {
        s.add(x);
    }
    EXPECT_FLOAT_EQ(s.min().value(), -3.0f);
    EXPECT_FLOAT_EQ(s.max().value(), -1.0f);
    EXPECT_FLOAT_EQ(s.mean().value(), -2.0f);
    EXPECT_NEAR(s.rms().value(), std::sqrt(14.0 / 3.0), 1e-5);
}

TEST(Statistics, MeanIsZeroButRmsShowsTheEnergyOfASymmetricSignal) {
    Statistics<> s;
    for (float x : {-1.0f, 1.0f, -1.0f, 1.0f}) {
        s.add(x);
    }
    EXPECT_NEAR(s.mean().value(), 0.0f, 1e-6f);
    EXPECT_FLOAT_EQ(s.rms().value(), 1.0f);
}

TEST(Statistics, ConstantSignal) {
    Statistics<> s;
    for (int i = 0; i < 1000; ++i) {
        s.add(5.0f);
    }
    EXPECT_FLOAT_EQ(s.min().value(), 5.0f);
    EXPECT_FLOAT_EQ(s.max().value(), 5.0f);
    EXPECT_FLOAT_EQ(s.mean().value(), 5.0f);
    EXPECT_FLOAT_EQ(s.rms().value(), 5.0f);
}

TEST(Statistics, RejectsNaNAndInfinity) {
    Statistics<> s;
    s.add(2.0f);
    EXPECT_FALSE(s.add(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(s.add(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(s.add(-std::numeric_limits<float>::infinity()));
    EXPECT_EQ(s.count(), 1u);
    EXPECT_FLOAT_EQ(s.mean().value(), 2.0f);
}

TEST(Statistics, OnlyRejectedSamplesLeavesItEmpty) {
    Statistics<> s;
    EXPECT_FALSE(s.add(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.mean(), std::nullopt);
}

TEST(Statistics, IgnoresInvalidSensorReadings) {
    Statistics<> s;
    EXPECT_TRUE(s.add(SensorReading{10.0f, 1}));
    EXPECT_FALSE(s.add(SensorReading::invalid(2)));
    EXPECT_TRUE(s.add(SensorReading{20.0f, 3}));
    EXPECT_EQ(s.count(), 2u);
    EXPECT_FLOAT_EQ(s.mean().value(), 15.0f);
}

TEST(Statistics, ManySmallValuesDoNotDrift) {
    Statistics<> s;  // double accumulators
    for (int i = 0; i < 100000; ++i) {
        s.add(0.1f);
    }
    EXPECT_NEAR(s.mean().value(), 0.1f, 1e-6f);
}

TEST(Statistics, LargeMagnitudesDoNotOverflowTheSumOfSquares) {
    Statistics<> s;
    s.add(1e18f);
    s.add(1e18f);
    EXPECT_FLOAT_EQ(s.rms().value(), 1e18f);  // 1e36 would overflow float
}

TEST(Statistics, ResetForgetsEverything) {
    Statistics<> s;
    s.add(1.0f);
    s.add(9.0f);
    s.reset();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.max(), std::nullopt);
    s.add(4.0f);
    EXPECT_FLOAT_EQ(s.min().value(), 4.0f);
    EXPECT_FLOAT_EQ(s.max().value(), 4.0f);
}

TEST(Statistics, WorksOverTheContentsOfARingBuffer) {
    RingBuffer<float, 3> buf;
    for (float x : {1.0f, 2.0f, 3.0f, 4.0f}) {
        buf.push(x);  // buffer now holds 2, 3, 4
    }
    Statistics<> s;
    for (std::size_t i = 0; i < buf.size(); ++i) {
        s.add(buf[i]);
    }
    EXPECT_FLOAT_EQ(s.min().value(), 2.0f);
    EXPECT_FLOAT_EQ(s.max().value(), 4.0f);
    EXPECT_FLOAT_EQ(s.mean().value(), 3.0f);
}

TEST(Statistics, DoubleValueTypeWorks) {
    Statistics<double> s;
    s.add(0.5);
    s.add(1.5);
    EXPECT_DOUBLE_EQ(s.mean().value(), 1.0);
}
