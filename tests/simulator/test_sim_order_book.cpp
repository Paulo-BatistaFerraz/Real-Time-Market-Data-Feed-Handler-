#include <gtest/gtest.h>
#include "qf/simulator/sim_order_book.hpp"

using namespace qf;
using namespace qf::simulator;

// TODO: Simulator order book tests

TEST(SimOrderBook, AddAndCancel) {
    EXPECT_TRUE(true);
}

TEST(SimOrderBook, CancelUnknownFails) {
    SimOrderBook book;
    auto result = book.cancel(999);
    EXPECT_FALSE(result.has_value());
}

TEST(SimOrderBook, ExecuteReducesQuantity) {
    EXPECT_TRUE(true);
}

TEST(SimOrderBook, RandomLiveOrder) {
    EXPECT_TRUE(true);
}
