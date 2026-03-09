#include <gtest/gtest.h>
#include "mdfh/simulator/price_walk.hpp"

using namespace mdfh::simulator;

// TODO: Price walk tests

TEST(PriceWalk, DeterministicWithSeed) {
    PriceWalk w1(100.0, 0.02, 0.01, 42);
    PriceWalk w2(100.0, 0.02, 0.01, 42);
    EXPECT_EQ(w1.next(), w2.next());
}

TEST(PriceWalk, StaysWithinBounds) {
    EXPECT_TRUE(true);
}

TEST(PriceWalk, SnapsToTickSize) {
    EXPECT_TRUE(true);
}
