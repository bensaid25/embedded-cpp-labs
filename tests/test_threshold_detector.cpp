#include <gtest/gtest.h>

#include <limits>

#include <ecl/sensor_reading.hpp>
#include <ecl/threshold_detector.hpp>

using ecl::SensorReading;
using ecl::ThresholdDetector;
using ecl::ThresholdState;

namespace {

constexpr bool compile_time_check() {
    auto det = ThresholdDetector<>::make(0.0f, 10.0f, 1.0f);
    if (!det) {
        return false;
    }
    det->update(11.0f);
    return det->state() == ThresholdState::High;
}
static_assert(compile_time_check());

ThresholdDetector<> make_or_die(float low, float high, float h = 0.0f) {
    auto det = ThresholdDetector<>::make(low, high, h);
    EXPECT_TRUE(det.has_value());
    return *det;
}

}  // namespace

// ---- configuration -------------------------------------------------------

TEST(ThresholdDetector, MakeAcceptsAValidConfiguration) {
    auto det = ThresholdDetector<>::make(0.0f, 10.0f, 2.0f);
    ASSERT_TRUE(det.has_value());
    EXPECT_FLOAT_EQ(det->low(), 0.0f);
    EXPECT_FLOAT_EQ(det->high(), 10.0f);
    EXPECT_FLOAT_EQ(det->hysteresis(), 2.0f);
}

TEST(ThresholdDetector, MakeRejectsLowAboveHigh) {
    EXPECT_FALSE(ThresholdDetector<>::make(10.0f, 0.0f).has_value());
}

TEST(ThresholdDetector, MakeRejectsNegativeHysteresis) {
    EXPECT_FALSE(ThresholdDetector<>::make(0.0f, 10.0f, -1.0f).has_value());
}

TEST(ThresholdDetector, MakeRejectsHysteresisWiderThanTheBand) {
    EXPECT_FALSE(ThresholdDetector<>::make(0.0f, 10.0f, 10.5f).has_value());
    EXPECT_TRUE(ThresholdDetector<>::make(0.0f, 10.0f, 10.0f).has_value());
}

TEST(ThresholdDetector, MakeRejectsNonFiniteConfiguration) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(ThresholdDetector<>::make(nan, 10.0f).has_value());
    EXPECT_FALSE(ThresholdDetector<>::make(0.0f, nan).has_value());
    EXPECT_FALSE(ThresholdDetector<>::make(-inf, 10.0f).has_value());
    EXPECT_FALSE(ThresholdDetector<>::make(0.0f, inf).has_value());
    EXPECT_FALSE(ThresholdDetector<>::make(0.0f, 10.0f, nan).has_value());
}

TEST(ThresholdDetector, ZeroWidthBandIsAllowed) {
    auto det = make_or_die(5.0f, 5.0f);
    det.update(5.0f);
    EXPECT_EQ(det.state(), ThresholdState::Normal);
    det.update(5.1f);
    EXPECT_EQ(det.state(), ThresholdState::High);
    det.update(4.9f);
    EXPECT_EQ(det.state(), ThresholdState::Low);
}

// ---- basic detection (no hysteresis) --------------------------------------

TEST(ThresholdDetector, StartsNormal) {
    auto det = make_or_die(0.0f, 10.0f);
    EXPECT_EQ(det.state(), ThresholdState::Normal);
    EXPECT_FALSE(det.is_alarm());
}

TEST(ThresholdDetector, ValuesInsideTheLimitsStayNormal) {
    auto det = make_or_die(0.0f, 10.0f);
    EXPECT_FALSE(det.update(5.0f));
    EXPECT_FALSE(det.update(0.1f));
    EXPECT_FALSE(det.update(9.9f));
    EXPECT_EQ(det.state(), ThresholdState::Normal);
}

TEST(ThresholdDetector, ValuesExactlyOnTheLimitsAreNormal) {
    auto det = make_or_die(0.0f, 10.0f);
    EXPECT_FALSE(det.update(10.0f));
    EXPECT_FALSE(det.update(0.0f));
    EXPECT_EQ(det.state(), ThresholdState::Normal);
}

TEST(ThresholdDetector, AboveHighRaisesHighAndReportsTheChangeOnce) {
    auto det = make_or_die(0.0f, 10.0f);
    EXPECT_TRUE(det.update(10.5f));
    EXPECT_EQ(det.state(), ThresholdState::High);
    EXPECT_TRUE(det.is_alarm());
    EXPECT_FALSE(det.update(11.0f));  // still High: no new change
}

TEST(ThresholdDetector, BelowLowRaisesLow) {
    auto det = make_or_die(0.0f, 10.0f);
    EXPECT_TRUE(det.update(-0.5f));
    EXPECT_EQ(det.state(), ThresholdState::Low);
}

TEST(ThresholdDetector, ReturnsToNormalWhenBackInsideWithoutHysteresis) {
    auto det = make_or_die(0.0f, 10.0f);
    det.update(12.0f);
    EXPECT_TRUE(det.update(10.0f));  // exactly on the limit clears it
    EXPECT_EQ(det.state(), ThresholdState::Normal);
    det.update(-1.0f);
    EXPECT_TRUE(det.update(0.0f));
    EXPECT_EQ(det.state(), ThresholdState::Normal);
}

// ---- hysteresis -----------------------------------------------------------

TEST(ThresholdDetector, HighClearsOnlyAfterDroppingByTheHysteresis) {
    auto det = make_or_die(0.0f, 10.0f, 2.0f);
    det.update(11.0f);
    EXPECT_EQ(det.state(), ThresholdState::High);
    EXPECT_FALSE(det.update(9.5f));  // below the limit but not by 2 yet
    EXPECT_EQ(det.state(), ThresholdState::High);
    EXPECT_FALSE(det.update(8.5f));
    EXPECT_EQ(det.state(), ThresholdState::High);
    EXPECT_TRUE(det.update(8.0f));   // 10 - 2: clears
    EXPECT_EQ(det.state(), ThresholdState::Normal);
    EXPECT_TRUE(det.update(10.5f));  // and can trigger again
    EXPECT_EQ(det.state(), ThresholdState::High);
}

TEST(ThresholdDetector, LowClearsOnlyAfterRisingByTheHysteresis) {
    auto det = make_or_die(0.0f, 10.0f, 2.0f);
    det.update(-1.0f);
    EXPECT_EQ(det.state(), ThresholdState::Low);
    EXPECT_FALSE(det.update(1.5f));
    EXPECT_EQ(det.state(), ThresholdState::Low);
    EXPECT_TRUE(det.update(2.0f));   // 0 + 2: clears
    EXPECT_EQ(det.state(), ThresholdState::Normal);
}

TEST(ThresholdDetector, HysteresisStopsFlickeringOnANoisySignalNearALimit) {
    const float noisy[] = {10.5f, 9.9f, 10.2f, 9.8f, 10.1f};

    auto plain = make_or_die(0.0f, 10.0f, 0.0f);
    auto steady = make_or_die(0.0f, 10.0f, 1.0f);
    int plain_changes = 0;
    int steady_changes = 0;
    for (float x : noisy) {
        plain_changes += plain.update(x) ? 1 : 0;
        steady_changes += steady.update(x) ? 1 : 0;
    }
    EXPECT_EQ(plain_changes, 5);   // flickers on every sample
    EXPECT_EQ(steady_changes, 1);  // raised once, stays raised
    EXPECT_EQ(steady.state(), ThresholdState::High);
}

TEST(ThresholdDetector, HysteresisEqualToTheBandWidth) {
    auto det = make_or_die(0.0f, 10.0f, 10.0f);
    det.update(11.0f);
    EXPECT_EQ(det.state(), ThresholdState::High);
    EXPECT_FALSE(det.update(1.0f));  // not yet at 10 - 10 = 0
    EXPECT_TRUE(det.update(0.0f));
    EXPECT_EQ(det.state(), ThresholdState::Normal);  // 0 is not below low
}

TEST(ThresholdDetector, JumpingAcrossTheWholeBandChangesStateDirectly) {
    auto det = make_or_die(0.0f, 10.0f, 2.0f);
    det.update(11.0f);
    EXPECT_TRUE(det.update(-5.0f));
    EXPECT_EQ(det.state(), ThresholdState::Low);
    EXPECT_TRUE(det.update(20.0f));
    EXPECT_EQ(det.state(), ThresholdState::High);
}

// ---- bad input ------------------------------------------------------------

TEST(ThresholdDetector, NonFiniteValuesAreIgnored) {
    auto det = make_or_die(0.0f, 10.0f, 1.0f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    EXPECT_FALSE(det.update(nan));
    EXPECT_FALSE(det.update(inf));
    EXPECT_EQ(det.state(), ThresholdState::Normal);

    det.update(11.0f);
    EXPECT_FALSE(det.update(nan));
    EXPECT_FALSE(det.update(-inf));
    EXPECT_EQ(det.state(), ThresholdState::High);  // unchanged
}

TEST(ThresholdDetector, InvalidSensorReadingsAreIgnored) {
    auto det = make_or_die(0.0f, 10.0f);
    EXPECT_FALSE(det.update(SensorReading::invalid(1)));
    EXPECT_EQ(det.state(), ThresholdState::Normal);
    EXPECT_TRUE(det.update(SensorReading{12.0f, 2}));
    EXPECT_EQ(det.state(), ThresholdState::High);
}

// ---- other ----------------------------------------------------------------

TEST(ThresholdDetector, ResetReturnsToNormal) {
    auto det = make_or_die(0.0f, 10.0f, 2.0f);
    det.update(11.0f);
    det.reset();
    EXPECT_EQ(det.state(), ThresholdState::Normal);
    EXPECT_FALSE(det.is_alarm());
}

TEST(ThresholdDetector, DoubleValueTypeWorks) {
    auto det = ThresholdDetector<double>::make(-1.0, 1.0, 0.5);
    ASSERT_TRUE(det.has_value());
    EXPECT_TRUE(det->update(1.5));
    EXPECT_EQ(det->state(), ThresholdState::High);
    EXPECT_FALSE(det->update(0.8));
    EXPECT_TRUE(det->update(0.5));
    EXPECT_EQ(det->state(), ThresholdState::Normal);
}
