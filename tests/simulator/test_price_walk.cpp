#include <gtest/gtest.h>
#include "qf/simulator/price_walk.hpp"

using namespace qf;
using namespace qf::simulator;

TEST(PriceWalk, DeterministicWithSeed) {
    PriceWalk w1(100.0, 0.02, 0.01, 42);
    PriceWalk w2(100.0, 0.02, 0.01, 42);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(w1.next(), w2.next());
    }
}

TEST(PriceWalk, DifferentSeedsDiffer) {
    PriceWalk w1(100.0, 0.5, 0.01, 42);
    PriceWalk w2(100.0, 0.5, 0.01, 99);
    // At least one of 100 steps should differ
    bool found_diff = false;
    for (int i = 0; i < 100; ++i) {
        if (w1.next() != w2.next()) { found_diff = true; break; }
    }
    EXPECT_TRUE(found_diff);
}

TEST(PriceWalk, StaysWithinBounds) {
    PriceWalk walk(100.0, 0.5, 0.01, 123);
    for (int i = 0; i < 10000; ++i) {
        Price p = walk.next();
        double d = price_to_double(p);
        EXPECT_GT(d, 0.0) << "Price went non-positive at step " << i;
        EXPECT_LE(d, 200.0) << "Price exceeded 2x mean at step " << i;
    }
}

TEST(PriceWalk, SnapsToTickSize) {
    double tick = 0.05;
    PriceWalk walk(50.0, 0.02, tick, 7);
    for (int i = 0; i < 1000; ++i) {
        Price p = walk.next();
        double d = price_to_double(p);
        // d should be a multiple of tick_size (within floating-point tolerance)
        double remainder = std::fmod(d, tick);
        if (remainder > tick / 2.0) remainder = tick - remainder;
        EXPECT_NEAR(remainder, 0.0, 1e-4) << "Price " << d << " not on tick grid";
    }
}

TEST(PriceWalk, SmallDeltasPerTick) {
    PriceWalk walk(100.0, 0.02, 0.01, 55);
    Price prev = walk.current();
    for (int i = 0; i < 1000; ++i) {
        Price cur = walk.next();
        double delta = std::abs(price_to_double(cur) - price_to_double(prev));
        // With vol=0.02, dt=0.001 each step should be tiny (< $1)
        EXPECT_LT(delta, 1.0) << "Jump too large at step " << i;
        prev = cur;
    }
}

TEST(PriceWalk, MeanReverts) {
    // High mean-reversion: after many steps should be near mean
    PriceWalk walk(100.0, 0.001, 0.01, 77);
    for (int i = 0; i < 10000; ++i) {
        walk.next();
    }
    double final_price = walk.current_double();
    EXPECT_NEAR(final_price, 100.0, 5.0);
}
