#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <ecl/sensor_reading.hpp>
#include <vibration_pipeline.hpp>

using demo::VibrationPipeline;
using demo::WindowResult;
using ecl::SensorReading;
using ecl::ThresholdState;

namespace {

constexpr double kPi = 3.14159265358979323846;

// 1 g of gravity plus a sine of the given amplitude with a period of
// 8 samples. 8 divides the DC window (32) and the result window (200), so
// the DC estimate and the RMS are exact apart from rounding.
SensorReading sample(std::uint32_t i, float amplitude) {
    const float g =
        1.0f + amplitude * static_cast<float>(std::sin(2.0 * kPi * i / 8.0));
    return SensorReading{g, i * 5};
}

using Pipeline = VibrationPipeline<200, 32, 8>;

Pipeline make_or_die(float alert = 0.2f, float hysteresis = 0.05f) {
    auto p = Pipeline::make(alert, hysteresis);
    EXPECT_TRUE(p.has_value());
    return *p;
}

// Feeds `count` samples starting at index `start`; returns the results.
std::vector<WindowResult> feed(Pipeline& p, std::uint32_t start,
                               std::uint32_t count, float amplitude) {
    std::vector<WindowResult> results;
    for (std::uint32_t i = start; i < start + count; ++i) {
        if (const auto r = p.add(sample(i, amplitude))) {
            results.push_back(*r);
        }
    }
    return results;
}

}  // namespace

// ---- configuration --------------------------------------------------------

TEST(VibrationPipeline, MakeAcceptsAValidConfiguration) {
    EXPECT_TRUE(Pipeline::make(0.2f, 0.05f).has_value());
    EXPECT_TRUE(Pipeline::make(0.2f, 0.0f).has_value());
}

TEST(VibrationPipeline, MakeRejectsAnInvalidConfiguration) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(Pipeline::make(-0.1f, 0.0f).has_value());   // negative limit
    EXPECT_FALSE(Pipeline::make(0.2f, -0.05f).has_value());  // negative hysteresis
    EXPECT_FALSE(Pipeline::make(0.2f, 0.3f).has_value());    // hysteresis too wide
    EXPECT_FALSE(Pipeline::make(nan, 0.0f).has_value());
}

// ---- windows --------------------------------------------------------------

TEST(VibrationPipeline, FirstResultComesAfterWarmUpPlusOneWindow) {
    auto p = make_or_die();
    // 31 samples are needed before the DC estimate is full, and the 32nd
    // sample is the first one counted in a window: 31 + 200 samples in all.
    EXPECT_TRUE(feed(p, 0, 230, 0.0f).empty());
    const auto results = feed(p, 230, 1, 0.0f);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].index, 0u);
}

TEST(VibrationPipeline, ProducesOneResultPerWindowAfterwards) {
    auto p = make_or_die();
    const auto results = feed(p, 0, 31 + 200 * 4, 0.0f);
    ASSERT_EQ(results.size(), 4u);
    for (std::size_t k = 0; k < results.size(); ++k) {
        EXPECT_EQ(results[k].index, k);
    }
}

// ---- values ---------------------------------------------------------------

TEST(VibrationPipeline, ConstantGravityHasZeroRms) {
    auto p = make_or_die();
    const auto results = feed(p, 0, 31 + 200 * 2, 0.0f);
    ASSERT_EQ(results.size(), 2u);
    for (const auto& r : results) {
        EXPECT_NEAR(r.rms, 0.0f, 1e-5f);
        EXPECT_NEAR(r.peak, 0.0f, 1e-5f);
        EXPECT_EQ(r.state, ThresholdState::Normal);
        EXPECT_FALSE(r.state_changed);
    }
}

TEST(VibrationPipeline, SineOnTopOfGravityGivesTheExpectedRmsAndPeak) {
    auto p = make_or_die(10.0f, 0.0f);  // alert limit far away
    const auto results = feed(p, 0, 31 + 200 * 2, 0.5f);
    ASSERT_EQ(results.size(), 2u);
    for (const auto& r : results) {
        EXPECT_NEAR(r.rms, 0.5f / std::sqrt(2.0f), 2e-3f);  // 0.3536
        EXPECT_NEAR(r.peak, 0.5f, 2e-3f);
    }
}

TEST(VibrationPipeline, TheDcLevelDoesNotMatter) {
    // Same vibration, different gravity component: the RMS must not change.
    auto p = make_or_die(10.0f, 0.0f);
    std::vector<WindowResult> results;
    for (std::uint32_t i = 0; i < 31 + 200; ++i) {
        const float g = 3.0f + 0.5f * static_cast<float>(
                                   std::sin(2.0 * kPi * i / 8.0));
        if (const auto r = p.add(SensorReading{g, i * 5})) {
            results.push_back(*r);
        }
    }
    ASSERT_EQ(results.size(), 1u);
    EXPECT_NEAR(results[0].rms, 0.5f / std::sqrt(2.0f), 2e-3f);
}

// ---- alert ----------------------------------------------------------------

TEST(VibrationPipeline, RaisesTheAlertOnceAndClearsItWithHysteresis) {
    auto p = make_or_die(0.2f, 0.05f);

    // Rest, then a long vibration, then rest again.
    std::vector<WindowResult> all;
    auto add = [&](std::uint32_t start, std::uint32_t count, float amp) {
        const auto r = feed(p, start, count, amp);
        all.insert(all.end(), r.begin(), r.end());
    };
    add(0, 400, 0.0f);
    add(400, 800, 0.5f);
    add(1200, 800, 0.0f);

    int raised = 0;
    int cleared = 0;
    for (const auto& r : all) {
        if (r.state_changed && r.state == ThresholdState::High) {
            ++raised;
        }
        if (r.state_changed && r.state == ThresholdState::Normal) {
            ++cleared;
        }
    }
    EXPECT_EQ(raised, 1);
    EXPECT_EQ(cleared, 1);
    EXPECT_EQ(all.back().state, ThresholdState::Normal);
    EXPECT_EQ(p.state(), ThresholdState::Normal);
}

// ---- bad input ------------------------------------------------------------

TEST(VibrationPipeline, InvalidReadingsAreSkippedAndCounted) {
    auto clean = make_or_die();
    auto noisy = make_or_die();

    std::vector<WindowResult> a;
    std::vector<WindowResult> b;
    std::size_t invalid_count = 0;
    for (std::uint32_t i = 0; i < 31 + 200 * 2; ++i) {
        if (const auto r = clean.add(sample(i, 0.5f))) {
            a.push_back(*r);
        }
        if (const auto r = noisy.add(sample(i, 0.5f))) {
            b.push_back(*r);
        }
        if (i % 7 == 0) {  // a failed reading between two good ones
            const auto none = noisy.add(SensorReading::invalid(i * 5));
            EXPECT_FALSE(none.has_value());
            ++invalid_count;
        }
    }
    EXPECT_EQ(noisy.skipped(), invalid_count);
    EXPECT_EQ(clean.skipped(), 0u);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t k = 0; k < a.size(); ++k) {
        EXPECT_FLOAT_EQ(a[k].rms, b[k].rms);
    }
}

TEST(VibrationPipeline, NaNReadingsBecomeInvalidAndAreSkipped) {
    auto p = make_or_die();
    const SensorReading bad{std::numeric_limits<float>::quiet_NaN(), 0};
    EXPECT_FALSE(p.add(bad).has_value());
    EXPECT_EQ(p.skipped(), 1u);
}

// ---- history --------------------------------------------------------------

TEST(VibrationPipeline, HistoryKeepsTheRecentWindowRms) {
    auto p = make_or_die(10.0f, 0.0f);
    const auto results = feed(p, 0, 31 + 200 * 3, 0.5f);
    ASSERT_EQ(results.size(), 3u);
    ASSERT_EQ(p.history().size(), 3u);
    for (std::size_t k = 0; k < 3; ++k) {
        EXPECT_FLOAT_EQ(p.history()[k], results[k].rms);
    }
}

TEST(VibrationPipeline, HistoryKeepsOnlyTheLastEightWindows) {
    auto p = make_or_die(10.0f, 0.0f);
    const auto results = feed(p, 0, 31 + 200 * 10, 0.5f);
    ASSERT_EQ(results.size(), 10u);
    ASSERT_EQ(p.history().size(), 8u);
    EXPECT_FLOAT_EQ(p.history()[0], results[2].rms);  // oldest kept
    EXPECT_FLOAT_EQ(p.history()[7], results[9].rms);  // newest
}
