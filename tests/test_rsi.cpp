#include <gtest/gtest.h>
#include "qf/signals/indicators/rsi.hpp"

using namespace qf::signals;

// Test: default constructor (period=14)
TEST(RSITest, DefaultConstructor) {
    RSI rsi;
    EXPECT_EQ(rsi.period(), 14);
    EXPECT_EQ(rsi.name(), "rsi_14");
}

// Test: custom period
TEST(RSITest, CustomPeriod) {
    RSI rsi(7);
    EXPECT_EQ(rsi.period(), 7);
    EXPECT_EQ(rsi.name(), "rsi_7");
}

// Test: returns 50 during warm-up
TEST(RSITest, WarmUpReturns50) {
    RSI rsi(14);
    double v = rsi.update(100.0);
    EXPECT_DOUBLE_EQ(v, 50.0);
    EXPECT_FALSE(rsi.ready());

    // Feed 13 more prices (total 14) — still one short of ready (need period+1)
    for (int i = 0; i < 13; ++i) {
        v = rsi.update(100.0 + static_cast<double>(i));
    }
    EXPECT_FALSE(rsi.ready());
    EXPECT_DOUBLE_EQ(v, 50.0);
}

// Test: all gains → RSI = 100
TEST(RSITest, AllGainsRSI100) {
    RSI rsi(5);
    // 6 prices: 10, 20, 30, 40, 50, 60 — all gains
    rsi.update(10.0);
    rsi.update(20.0);
    rsi.update(30.0);
    rsi.update(40.0);
    rsi.update(50.0);
    double v = rsi.update(60.0);
    EXPECT_DOUBLE_EQ(v, 100.0);
    EXPECT_TRUE(rsi.is_overbought());
    EXPECT_FALSE(rsi.is_oversold());
}

// Test: all losses → RSI = 0
TEST(RSITest, AllLossesRSI0) {
    RSI rsi(5);
    // 6 prices: 60, 50, 40, 30, 20, 10 — all losses
    rsi.update(60.0);
    rsi.update(50.0);
    rsi.update(40.0);
    rsi.update(30.0);
    rsi.update(20.0);
    double v = rsi.update(10.0);
    EXPECT_DOUBLE_EQ(v, 0.0);
    EXPECT_TRUE(rsi.is_oversold());
    EXPECT_FALSE(rsi.is_overbought());
}

// Test: known hand-calculated RSI with period=3
TEST(RSITest, HandCalculatedPeriod3) {
    RSI rsi(3);
    // Prices: 44, 44.34, 44.09, 43.61
    // Deltas:     +0.34,  -0.25,  -0.48
    // Gains:       0.34,   0.00,   0.00  → avg_gain = 0.34/3
    // Losses:      0.00,   0.25,   0.48  → avg_loss = 0.73/3
    rsi.update(44.0);
    rsi.update(44.34);
    rsi.update(44.09);
    double v = rsi.update(43.61);

    double avg_gain = 0.34 / 3.0;
    double avg_loss = 0.73 / 3.0;
    double rs = avg_gain / avg_loss;
    double expected = 100.0 - 100.0 / (1.0 + rs);
    EXPECT_NEAR(v, expected, 1e-9);
    EXPECT_TRUE(rsi.ready());
}

// Test: Wilder smoothing after warm-up
TEST(RSITest, WilderSmoothing) {
    RSI rsi(3);
    // 4 prices for initial: 10, 13, 11, 14  → deltas: +3, -2, +3
    // avg_gain = (3+0+3)/3 = 2.0, avg_loss = (0+2+0)/3 = 0.6667
    rsi.update(10.0);
    rsi.update(13.0);
    rsi.update(11.0);
    rsi.update(14.0);

    double avg_gain = 2.0;
    double avg_loss = 2.0 / 3.0;

    // Next price: 12 → delta = -2, gain=0, loss=2
    // smoothed avg_gain = (2.0*2 + 0)/3 = 4/3
    // smoothed avg_loss = (2/3*2 + 2)/3 = (4/3 + 2)/3 = (10/3)/3 = 10/9
    double v = rsi.update(12.0);

    avg_gain = (avg_gain * 2.0 + 0.0) / 3.0;
    avg_loss = (avg_loss * 2.0 + 2.0) / 3.0;
    double rs = avg_gain / avg_loss;
    double expected = 100.0 - 100.0 / (1.0 + rs);
    EXPECT_NEAR(v, expected, 1e-9);
}

// Test: RSI stays in [0, 100] range with many random-like values
TEST(RSITest, StaysInRange) {
    RSI rsi(14);
    double prices[] = {100, 102, 99, 101, 98, 103, 97, 105,
                       96, 104, 95, 106, 94, 107, 93, 108,
                       92, 109, 91, 110};
    for (double p : prices) {
        double v = rsi.update(p);
        EXPECT_GE(v, 0.0);
        EXPECT_LE(v, 100.0);
    }
}

// Test: value() returns current RSI without consuming data
TEST(RSITest, ValueDoesNotConsume) {
    RSI rsi(3);
    rsi.update(10.0);
    rsi.update(20.0);
    rsi.update(30.0);
    rsi.update(40.0);
    double v1 = rsi.value();
    double v2 = rsi.value();
    EXPECT_DOUBLE_EQ(v1, v2);
}

// Test: reset clears state
TEST(RSITest, ResetClearsState) {
    RSI rsi(3);
    rsi.update(10.0);
    rsi.update(20.0);
    rsi.update(30.0);
    rsi.update(40.0);
    EXPECT_TRUE(rsi.ready());

    rsi.reset();
    EXPECT_FALSE(rsi.ready());
    EXPECT_DOUBLE_EQ(rsi.value(), 50.0);

    // After reset, warm-up restarts
    double v = rsi.update(100.0);
    EXPECT_DOUBLE_EQ(v, 50.0);
}

// Test: overbought/oversold thresholds
TEST(RSITest, OverboughtOversoldThresholds) {
    RSI rsi(3);
    // Not overbought or oversold at start
    EXPECT_FALSE(rsi.is_overbought());
    EXPECT_FALSE(rsi.is_oversold());
}

// Test: flat prices → RSI = 50 (no gains, no losses after first)
TEST(RSITest, FlatPricesRSI) {
    RSI rsi(3);
    // All same price → all deltas are 0 → avg_gain=0, avg_loss=0
    rsi.update(50.0);
    rsi.update(50.0);
    rsi.update(50.0);
    double v = rsi.update(50.0);
    // avg_gain=0, avg_loss=0 → special case: RSI = 100 (no loss means infinite RS)
    // Actually: avg_loss == 0 so rsi = 100
    EXPECT_DOUBLE_EQ(v, 100.0);
}
