#include <gtest/gtest.h>
#include "qf/signals/indicators/zscore.hpp"
#include <cmath>

using namespace qf::signals;

// Test: default window is 20
TEST(ZScoreTest, DefaultWindow) {
    ZScore zs;
    EXPECT_EQ(zs.window(), 20);
}

// Test: custom window
TEST(ZScoreTest, CustomWindow) {
    ZScore zs(50);
    EXPECT_EQ(zs.window(), 50);
}

// Test: returns 0 with fewer than 2 samples
TEST(ZScoreTest, InsufficientDataReturnsZero) {
    ZScore zs(5);
    EXPECT_DOUBLE_EQ(zs.update(10.0), 0.0);
    EXPECT_FALSE(zs.ready());
}

// Test: z-score of constant values is 0 (zero stddev)
TEST(ZScoreTest, ConstantValuesReturnZero) {
    ZScore zs(5);
    for (int i = 0; i < 10; ++i) {
        EXPECT_DOUBLE_EQ(zs.update(42.0), 0.0);
    }
}

// Test: hand-calculated z-score with known values
TEST(ZScoreTest, HandCalculated) {
    ZScore zs(3);

    // val=2: count=1 → z=0
    EXPECT_DOUBLE_EQ(zs.update(2.0), 0.0);

    // val=4: count=2, mean=3, m2 = sum of (x-mean)^2 = 1+1 = 2
    //   variance = 2/2 = 1, stddev = 1
    //   z = (4 - 3) / 1 = 1.0
    EXPECT_NEAR(zs.update(4.0), 1.0, 1e-12);

    // val=6: count=3, values=[2,4,6], mean=4, var = ((4+0+4)/3) = 8/3
    //   stddev = sqrt(8/3), z = (6-4) / sqrt(8/3) = 2 / sqrt(8/3)
    double expected_var = 8.0 / 3.0;
    double expected_z = 2.0 / std::sqrt(expected_var);
    EXPECT_NEAR(zs.update(6.0), expected_z, 1e-10);
}

// Test: window rolls correctly — oldest value drops off
TEST(ZScoreTest, WindowRolling) {
    ZScore zs(3);

    // Fill window: 10, 20, 30
    zs.update(10.0);
    zs.update(20.0);
    zs.update(30.0);

    // Now add 40 — window becomes [20, 30, 40]
    // mean = 30, values: 20,30,40 → var = (100+0+100)/3 = 200/3
    // z = (40 - 30) / sqrt(200/3) = 10 / sqrt(200/3)
    double val = zs.update(40.0);
    double expected_var = 200.0 / 3.0;
    double expected_z = 10.0 / std::sqrt(expected_var);
    EXPECT_NEAR(val, expected_z, 1e-10);
}

// Test: value() returns last z-score without consuming data
TEST(ZScoreTest, ValueDoesNotConsume) {
    ZScore zs(5);
    zs.update(10.0);
    zs.update(20.0);
    double v = zs.update(30.0);
    EXPECT_DOUBLE_EQ(zs.value(), v);
    EXPECT_DOUBLE_EQ(zs.value(), v);  // unchanged
}

// Test: reset clears state
TEST(ZScoreTest, ResetClearsState) {
    ZScore zs(3);
    zs.update(10.0);
    zs.update(20.0);
    zs.update(30.0);
    zs.reset();

    EXPECT_FALSE(zs.ready());
    EXPECT_DOUBLE_EQ(zs.value(), 0.0);

    // After reset, first value returns 0 again
    EXPECT_DOUBLE_EQ(zs.update(100.0), 0.0);
}

// Test: name includes window
TEST(ZScoreTest, NameIncludesWindow) {
    ZScore zs(20);
    EXPECT_EQ(zs.name(), "zscore_20");
}

// Test: symmetric — value below mean gives negative z-score
TEST(ZScoreTest, NegativeZScore) {
    ZScore zs(3);
    // values: 10, 20, 30 → mean=20
    zs.update(10.0);
    zs.update(20.0);
    zs.update(30.0);

    // Now add 5 — window becomes [20, 30, 5], mean=55/3 ≈ 18.33
    // 5 is well below mean → negative z-score
    double val = zs.update(5.0);
    EXPECT_LT(val, 0.0);
}

// Test: Welford's algorithm stability with large values
TEST(ZScoreTest, NumericalStabilityLargeValues) {
    ZScore zs(5);
    double base = 1e9;
    for (int i = 0; i < 20; ++i) {
        double v = base + i * 0.01;
        double z = zs.update(v);
        // Should produce finite, reasonable z-scores
        EXPECT_TRUE(std::isfinite(z));
    }
}
