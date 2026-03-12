#include <gtest/gtest.h>
#include "qf/signals/indicators/ema.hpp"

using namespace qf::signals;

// Test: constructor computes correct alpha
TEST(EMATest, AlphaComputation) {
    // span=9 → alpha = 2/(9+1) = 0.2
    EMA ema(9);
    // Feed two values and verify the formula holds
    ema.update(10.0);
    double v = ema.update(20.0);
    // ema = 0.2 * 20 + 0.8 * 10 = 4 + 8 = 12
    EXPECT_DOUBLE_EQ(v, 12.0);
}

// Test: first value initializes EMA directly
TEST(EMATest, FirstValueInitializes) {
    EMA ema(10);
    double v = ema.update(42.0);
    EXPECT_DOUBLE_EQ(v, 42.0);
    EXPECT_DOUBLE_EQ(ema.value(), 42.0);
}

// Test: known sequence matches hand-calculated EMA
TEST(EMATest, KnownSequenceHandCalculated) {
    // span=3 → alpha = 2/(3+1) = 0.5
    EMA ema(3);

    // Step 1: value=2.0 → ema = 2.0 (initialization)
    EXPECT_DOUBLE_EQ(ema.update(2.0), 2.0);

    // Step 2: value=4.0 → ema = 0.5*4 + 0.5*2 = 3.0
    EXPECT_DOUBLE_EQ(ema.update(4.0), 3.0);

    // Step 3: value=6.0 → ema = 0.5*6 + 0.5*3 = 4.5
    EXPECT_DOUBLE_EQ(ema.update(6.0), 4.5);

    // Step 4: value=8.0 → ema = 0.5*8 + 0.5*4.5 = 6.25
    EXPECT_DOUBLE_EQ(ema.update(8.0), 6.25);

    // Step 5: value=10.0 → ema = 0.5*10 + 0.5*6.25 = 8.125
    EXPECT_DOUBLE_EQ(ema.update(10.0), 8.125);

    EXPECT_DOUBLE_EQ(ema.value(), 8.125);
}

// Test: value() returns current EMA without consuming data
TEST(EMATest, ValueDoesNotConsume) {
    EMA ema(5);
    ema.update(100.0);
    EXPECT_DOUBLE_EQ(ema.value(), 100.0);
    EXPECT_DOUBLE_EQ(ema.value(), 100.0);  // unchanged
}

// Test: reset clears state
TEST(EMATest, ResetClearsState) {
    EMA ema(3);
    ema.update(10.0);
    ema.update(20.0);
    ema.reset();

    // After reset, next update should initialize again
    EXPECT_DOUBLE_EQ(ema.update(50.0), 50.0);
}

// Test: name returns "ema_<span>"
TEST(EMATest, NameIncludesSpan) {
    EMA ema(20);
    EXPECT_EQ(ema.name(), "ema_20");
}

// Test: longer sequence with span=5, alpha = 2/6 = 1/3
TEST(EMATest, Span5Sequence) {
    EMA ema(5);  // alpha = 2/6 = 1/3
    const double alpha = 1.0 / 3.0;

    double expected = 10.0;
    EXPECT_DOUBLE_EQ(ema.update(10.0), expected);

    expected = alpha * 20.0 + (1.0 - alpha) * expected;
    EXPECT_NEAR(ema.update(20.0), expected, 1e-12);

    expected = alpha * 15.0 + (1.0 - alpha) * expected;
    EXPECT_NEAR(ema.update(15.0), expected, 1e-12);

    expected = alpha * 25.0 + (1.0 - alpha) * expected;
    EXPECT_NEAR(ema.update(25.0), expected, 1e-12);

    EXPECT_NEAR(ema.value(), expected, 1e-12);
}
