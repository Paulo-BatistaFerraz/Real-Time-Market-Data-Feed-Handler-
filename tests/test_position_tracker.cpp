#include <gtest/gtest.h>
#include "qf/strategy/position/position_tracker.hpp"
#include "qf/common/types.hpp"

using namespace qf;
using namespace qf::strategy;

class PositionTrackerTest : public ::testing::Test {
protected:
    PositionTracker tracker;
    Symbol sym_aapl{"AAPL"};
    Symbol sym_msft{"MSFT"};
};

// Test: initial position is zero
TEST_F(PositionTrackerTest, InitialPositionIsZero) {
    EXPECT_EQ(tracker.get(sym_aapl), 0);
}

// Test: buy increases position
TEST_F(PositionTrackerTest, BuyIncreasesPosition) {
    tracker.update(sym_aapl, Side::Buy, 100);
    EXPECT_EQ(tracker.get(sym_aapl), 100);
}

// Test: sell decreases position
TEST_F(PositionTrackerTest, SellDecreasesPosition) {
    tracker.update(sym_aapl, Side::Sell, 50);
    EXPECT_EQ(tracker.get(sym_aapl), -50);
}

// Test: multiple buys accumulate
TEST_F(PositionTrackerTest, MultipleBuysAccumulate) {
    tracker.update(sym_aapl, Side::Buy, 100);
    tracker.update(sym_aapl, Side::Buy, 200);
    tracker.update(sym_aapl, Side::Buy, 50);
    EXPECT_EQ(tracker.get(sym_aapl), 350);
}

// Test: buy then sell tracks net position
TEST_F(PositionTrackerTest, BuyThenSellTracksNet) {
    tracker.update(sym_aapl, Side::Buy, 300);
    tracker.update(sym_aapl, Side::Sell, 100);
    EXPECT_EQ(tracker.get(sym_aapl), 200);
}

// Test: sell more than bought goes short
TEST_F(PositionTrackerTest, SellMoreThanBoughtGoesShort) {
    tracker.update(sym_aapl, Side::Buy, 100);
    tracker.update(sym_aapl, Side::Sell, 250);
    EXPECT_EQ(tracker.get(sym_aapl), -150);
}

// Test: round-trip returns to zero
TEST_F(PositionTrackerTest, RoundTripReturnsToZero) {
    tracker.update(sym_aapl, Side::Buy, 500);
    tracker.update(sym_aapl, Side::Sell, 500);
    EXPECT_EQ(tracker.get(sym_aapl), 0);
}

// Test: multiple symbols tracked independently
TEST_F(PositionTrackerTest, MultipleSymbolsIndependent) {
    tracker.update(sym_aapl, Side::Buy, 100);
    tracker.update(sym_msft, Side::Sell, 200);

    EXPECT_EQ(tracker.get(sym_aapl), 100);
    EXPECT_EQ(tracker.get(sym_msft), -200);
}

// Test: get_all returns all positions
TEST_F(PositionTrackerTest, GetAllReturnsAllPositions) {
    tracker.update(sym_aapl, Side::Buy, 100);
    tracker.update(sym_msft, Side::Sell, 50);

    const auto& all = tracker.get_all();
    EXPECT_EQ(all.size(), 2u);
    EXPECT_EQ(all.at(sym_aapl.as_key()), 100);
    EXPECT_EQ(all.at(sym_msft.as_key()), -50);
}

// Test: reset clears all positions
TEST_F(PositionTrackerTest, ResetClearsAll) {
    tracker.update(sym_aapl, Side::Buy, 100);
    tracker.update(sym_msft, Side::Sell, 200);
    tracker.reset();

    EXPECT_EQ(tracker.get(sym_aapl), 0);
    EXPECT_EQ(tracker.get(sym_msft), 0);
    EXPECT_TRUE(tracker.get_all().empty());
}

// Test: large position values
TEST_F(PositionTrackerTest, LargePositionValues) {
    tracker.update(sym_aapl, Side::Buy, 1000000);
    tracker.update(sym_aapl, Side::Buy, 1000000);
    EXPECT_EQ(tracker.get(sym_aapl), 2000000);
}
