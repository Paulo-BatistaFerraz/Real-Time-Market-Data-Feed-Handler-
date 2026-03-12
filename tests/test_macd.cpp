#include <gtest/gtest.h>
#include "qf/signals/indicators/macd.hpp"
#include <cmath>

using namespace qf::signals;

// Test: default constructor uses 12/26/9
TEST(MACDTest, DefaultPeriods) {
    MACD macd;
    EXPECT_EQ(macd.fast_period(), 12);
    EXPECT_EQ(macd.slow_period(), 26);
    EXPECT_EQ(macd.signal_period(), 9);
}

// Test: custom periods
TEST(MACDTest, CustomPeriods) {
    MACD macd(5, 10, 3);
    EXPECT_EQ(macd.fast_period(), 5);
    EXPECT_EQ(macd.slow_period(), 10);
    EXPECT_EQ(macd.signal_period(), 3);
}

// Test: first update — both EMAs initialize to same price, MACD line = 0
TEST(MACDTest, FirstUpdateZero) {
    MACD macd;
    double v = macd.update(100.0);
    EXPECT_DOUBLE_EQ(v, 0.0);
    EXPECT_DOUBLE_EQ(macd.result().macd_line, 0.0);
    EXPECT_DOUBLE_EQ(macd.result().signal_line, 0.0);
    EXPECT_DOUBLE_EQ(macd.result().histogram, 0.0);
}

// Test: MACD uses two EMAs and signal EMA correctly
TEST(MACDTest, HandCalculatedSequence) {
    // fast=3 (alpha=0.5), slow=5 (alpha=1/3), signal=3 (alpha=0.5)
    MACD macd(3, 5, 3);

    // Step 1: price=10 → fast=10, slow=10, macd=0, signal=0, hist=0
    macd.update(10.0);
    EXPECT_DOUBLE_EQ(macd.result().macd_line, 0.0);

    // Step 2: price=20
    // fast = 0.5*20 + 0.5*10 = 15
    // slow = (1/3)*20 + (2/3)*10 = 40/3
    // macd = 15 - 40/3 = 5/3
    // signal = 0.5*(5/3) + 0.5*0 = 5/6
    // hist = 5/3 - 5/6 = 5/6
    macd.update(20.0);
    double expected_macd = 15.0 - 40.0 / 3.0;
    EXPECT_NEAR(macd.result().macd_line, expected_macd, 1e-12);
    double expected_signal = 0.5 * expected_macd + 0.5 * 0.0;
    EXPECT_NEAR(macd.result().signal_line, expected_signal, 1e-12);
    EXPECT_NEAR(macd.result().histogram, expected_macd - expected_signal, 1e-12);
}

// Test: update returns macd_line, value() also returns macd_line
TEST(MACDTest, ValueMatchesMacdLine) {
    MACD macd(3, 5, 3);
    macd.update(10.0);
    macd.update(20.0);
    double v = macd.update(15.0);
    EXPECT_DOUBLE_EQ(v, macd.value());
    EXPECT_DOUBLE_EQ(v, macd.result().macd_line);
}

// Test: histogram = macd_line - signal_line always
TEST(MACDTest, HistogramIsConsistent) {
    MACD macd;
    double prices[] = {100, 102, 99, 105, 103, 101, 107, 110, 108, 112};
    for (double p : prices) {
        macd.update(p);
        const auto& r = macd.result();
        EXPECT_NEAR(r.histogram, r.macd_line - r.signal_line, 1e-12);
    }
}

// Test: reset clears state
TEST(MACDTest, ResetClearsState) {
    MACD macd(3, 5, 3);
    macd.update(100.0);
    macd.update(110.0);
    macd.update(120.0);
    macd.reset();

    // After reset, behaves like fresh
    macd.update(50.0);
    EXPECT_DOUBLE_EQ(macd.result().macd_line, 0.0);
    EXPECT_DOUBLE_EQ(macd.result().signal_line, 0.0);
    EXPECT_DOUBLE_EQ(macd.result().histogram, 0.0);
}

// Test: name includes all three periods
TEST(MACDTest, NameIncludesPeriods) {
    MACD macd(12, 26, 9);
    EXPECT_EQ(macd.name(), "macd_12_26_9");
}

// Test: trending price produces positive MACD
TEST(MACDTest, TrendingPricePositiveMacd) {
    MACD macd(3, 5, 3);
    // Feed steadily rising prices
    for (int i = 1; i <= 20; ++i) {
        macd.update(static_cast<double>(i));
    }
    // Fast EMA reacts quicker to uptrend → macd_line should be positive
    EXPECT_GT(macd.result().macd_line, 0.0);
}
