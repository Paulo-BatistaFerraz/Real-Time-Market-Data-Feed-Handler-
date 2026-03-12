#include <gtest/gtest.h>
#include "qf/signals/indicators/bollinger.hpp"
#include <cmath>

using namespace qf::signals;

// Test: default constructor (period=20, num_std=2.0)
TEST(BollingerTest, DefaultConstructor) {
    Bollinger bb;
    EXPECT_EQ(bb.period(), 20);
    EXPECT_DOUBLE_EQ(bb.num_std(), 2.0);
    EXPECT_EQ(bb.name(), "bollinger_20");
}

// Test: custom constructor
TEST(BollingerTest, CustomConstructor) {
    Bollinger bb(10, 1.5);
    EXPECT_EQ(bb.period(), 10);
    EXPECT_DOUBLE_EQ(bb.num_std(), 1.5);
    EXPECT_EQ(bb.name(), "bollinger_10");
}

// Test: single value — std=0 so upper==middle==lower
TEST(BollingerTest, SingleValue) {
    Bollinger bb(5, 2.0);
    double mid = bb.update(100.0);
    EXPECT_DOUBLE_EQ(mid, 100.0);

    auto r = bb.result();
    EXPECT_DOUBLE_EQ(r.middle, 100.0);
    EXPECT_DOUBLE_EQ(r.upper, 100.0);
    EXPECT_DOUBLE_EQ(r.lower, 100.0);
    EXPECT_DOUBLE_EQ(r.bandwidth, 0.0);
}

// Test: known 5-value sequence with period=5
TEST(BollingerTest, KnownSequencePeriod5) {
    Bollinger bb(5, 2.0);

    // Feed 5 values: 10, 20, 30, 40, 50
    bb.update(10.0);
    bb.update(20.0);
    bb.update(30.0);
    bb.update(40.0);
    double mid = bb.update(50.0);

    // SMA = (10+20+30+40+50)/5 = 30
    EXPECT_DOUBLE_EQ(mid, 30.0);

    // Population variance = ((10-30)^2 + (20-30)^2 + (30-30)^2 + (40-30)^2 + (50-30)^2) / 5
    // = (400 + 100 + 0 + 100 + 400) / 5 = 200
    // stddev = sqrt(200) ≈ 14.1421356
    double expected_std = std::sqrt(200.0);

    auto r = bb.result();
    EXPECT_DOUBLE_EQ(r.middle, 30.0);
    EXPECT_NEAR(r.upper, 30.0 + 2.0 * expected_std, 1e-9);
    EXPECT_NEAR(r.lower, 30.0 - 2.0 * expected_std, 1e-9);
}

// Test: rolling window drops oldest value
TEST(BollingerTest, RollingWindowDropsOldest) {
    Bollinger bb(3, 1.0);

    bb.update(10.0);
    bb.update(20.0);
    bb.update(30.0);
    // Window: [10, 20, 30], SMA = 20

    double mid = bb.update(40.0);
    // Window: [20, 30, 40], SMA = 30
    EXPECT_NEAR(mid, 30.0, 1e-9);
    EXPECT_TRUE(bb.ready());
}

// Test: %B computation
TEST(BollingerTest, PercentB) {
    Bollinger bb(3, 2.0);
    bb.update(10.0);
    bb.update(20.0);
    bb.update(30.0);

    auto r = bb.result();
    // %B = (price - lower) / (upper - lower)
    // price=30, which is above middle=20
    double band_width = r.upper - r.lower;
    double expected_pctb = (30.0 - r.lower) / band_width;
    EXPECT_NEAR(r.percent_b, expected_pctb, 1e-9);
}

// Test: bandwidth = (upper - lower) / middle
TEST(BollingerTest, Bandwidth) {
    Bollinger bb(3, 2.0);
    bb.update(100.0);
    bb.update(110.0);
    bb.update(120.0);

    auto r = bb.result();
    double expected_bw = (r.upper - r.lower) / r.middle;
    EXPECT_NEAR(r.bandwidth, expected_bw, 1e-9);
}

// Test: value() returns middle band
TEST(BollingerTest, ValueReturnsMiddle) {
    Bollinger bb(5, 2.0);
    bb.update(10.0);
    bb.update(20.0);
    bb.update(30.0);
    EXPECT_DOUBLE_EQ(bb.value(), bb.result().middle);
}

// Test: reset clears state
TEST(BollingerTest, ResetClearsState) {
    Bollinger bb(3, 2.0);
    bb.update(10.0);
    bb.update(20.0);
    bb.update(30.0);
    EXPECT_TRUE(bb.ready());

    bb.reset();
    EXPECT_FALSE(bb.ready());
    EXPECT_DOUBLE_EQ(bb.value(), 0.0);

    // After reset, should reinitialize cleanly
    double mid = bb.update(50.0);
    EXPECT_DOUBLE_EQ(mid, 50.0);
}

// Test: constant prices → zero standard deviation
TEST(BollingerTest, ConstantPricesZeroStd) {
    Bollinger bb(5, 2.0);
    for (int i = 0; i < 5; ++i) {
        bb.update(42.0);
    }
    auto r = bb.result();
    EXPECT_DOUBLE_EQ(r.middle, 42.0);
    EXPECT_DOUBLE_EQ(r.upper, 42.0);
    EXPECT_DOUBLE_EQ(r.lower, 42.0);
    EXPECT_DOUBLE_EQ(r.bandwidth, 0.0);
}

// Test: ready() returns false before window is full
TEST(BollingerTest, ReadyBeforeWindowFull) {
    Bollinger bb(5, 2.0);
    EXPECT_FALSE(bb.ready());
    bb.update(1.0);
    EXPECT_FALSE(bb.ready());
    bb.update(2.0);
    bb.update(3.0);
    bb.update(4.0);
    EXPECT_FALSE(bb.ready());
    bb.update(5.0);
    EXPECT_TRUE(bb.ready());
}
