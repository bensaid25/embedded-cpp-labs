#include <gtest/gtest.h>

#include <limits>
#include <type_traits>

#include <ecl/sensor_reading.hpp>

using ecl::SensorReading;

static_assert(std::is_trivially_copyable_v<SensorReading>,
              "SensorReading must be trivially copyable");

TEST(SensorReading, DefaultConstructedIsInvalidAndZero) {
    constexpr SensorReading r;
    EXPECT_FALSE(r.is_valid());
    EXPECT_EQ(r.value(), 0.0f);
    EXPECT_EQ(r.timestamp_ms(), 0u);
}

TEST(SensorReading, StoresValueAndTimestamp) {
    constexpr SensorReading r{21.5f, 1000};
    EXPECT_TRUE(r.is_valid());
    EXPECT_EQ(r.value(), 21.5f);
    EXPECT_EQ(r.timestamp_ms(), 1000u);
}

TEST(SensorReading, NegativeValueIsValid) {
    constexpr SensorReading r{-40.0f, 5};
    EXPECT_TRUE(r.is_valid());
    EXPECT_EQ(r.value(), -40.0f);
}

TEST(SensorReading, ExplicitlyInvalidDropsTheValue) {
    constexpr SensorReading r{5.0f, 10, false};
    EXPECT_FALSE(r.is_valid());
    EXPECT_EQ(r.value(), 0.0f);
    EXPECT_EQ(r.timestamp_ms(), 10u);
}

TEST(SensorReading, NaNBecomesInvalid) {
    const SensorReading r{std::numeric_limits<float>::quiet_NaN(), 7};
    EXPECT_FALSE(r.is_valid());
    EXPECT_EQ(r.value(), 0.0f);
}

TEST(SensorReading, InfinityBecomesInvalid) {
    const SensorReading pos{std::numeric_limits<float>::infinity(), 1};
    const SensorReading neg{-std::numeric_limits<float>::infinity(), 2};
    EXPECT_FALSE(pos.is_valid());
    EXPECT_FALSE(neg.is_valid());
}

TEST(SensorReading, InvalidFactoryKeepsTimestamp) {
    constexpr auto r = SensorReading::invalid(42);
    EXPECT_FALSE(r.is_valid());
    EXPECT_EQ(r.timestamp_ms(), 42u);
}

TEST(SensorReading, EqualityComparesAllFields) {
    EXPECT_EQ((SensorReading{1.0f, 1}), (SensorReading{1.0f, 1}));
    EXPECT_NE((SensorReading{1.0f, 1}), (SensorReading{2.0f, 1}));
    EXPECT_NE((SensorReading{1.0f, 1}), (SensorReading{1.0f, 2}));
    EXPECT_NE((SensorReading{1.0f, 1}), (SensorReading{1.0f, 1, false}));
}

TEST(SensorReading, TwoInvalidReadingsWithSameTimestampAreEqual) {
    const SensorReading a{std::numeric_limits<float>::quiet_NaN(), 3};
    const SensorReading b = SensorReading::invalid(3);
    EXPECT_EQ(a, b);
}

TEST(SensorReading, UsableAtCompileTime) {
    constexpr SensorReading r{3.0f, 9};
    static_assert(r.is_valid());
    static_assert(r.value() == 3.0f);
    SUCCEED();
}
