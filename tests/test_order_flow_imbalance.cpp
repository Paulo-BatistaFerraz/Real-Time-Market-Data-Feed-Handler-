#include <gtest/gtest.h>
#include "qf/signals/features/order_flow_imbalance.hpp"
#include "qf/common/types.hpp"
#include <cmath>

using namespace qf;
using namespace qf::signals;

class OFITest : public ::testing::Test {
protected:
    OrderFlowImbalance ofi{10'000'000'000ULL};  // 10s window
    core::OrderBook book{Symbol("AAPL")};
    TradeHistory trades;
};

// Test: all buy trades → OFI = +1
TEST_F(OFITest, AllBuysReturnsPositiveOne) {
    trades.add({Symbol("AAPL"), double_to_price(100.0), 100, Side::Buy, 1000});
    trades.add({Symbol("AAPL"), double_to_price(100.0), 200, Side::Buy, 2000});

    double result = ofi.compute(book, trades);
    EXPECT_DOUBLE_EQ(result, 1.0);
}

// Test: all sell trades → OFI = -1
TEST_F(OFITest, AllSellsReturnsNegativeOne) {
    trades.add({Symbol("AAPL"), double_to_price(100.0), 150, Side::Sell, 1000});
    trades.add({Symbol("AAPL"), double_to_price(100.0), 250, Side::Sell, 2000});

    double result = ofi.compute(book, trades);
    EXPECT_DOUBLE_EQ(result, -1.0);
}

// Test: equal buy and sell volumes → OFI = 0
TEST_F(OFITest, BalancedTradesReturnsZero) {
    trades.add({Symbol("AAPL"), double_to_price(100.0), 100, Side::Buy, 1000});
    trades.add({Symbol("AAPL"), double_to_price(100.0), 100, Side::Sell, 2000});

    double result = ofi.compute(book, trades);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test: known imbalance ratio
TEST_F(OFITest, KnownImbalanceRatio) {
    // buy=300, sell=100 → OFI = (300-100)/400 = 0.5
    trades.add({Symbol("AAPL"), double_to_price(100.0), 200, Side::Buy, 1000});
    trades.add({Symbol("AAPL"), double_to_price(100.0), 100, Side::Buy, 2000});
    trades.add({Symbol("AAPL"), double_to_price(100.0), 100, Side::Sell, 3000});

    double result = ofi.compute(book, trades);
    EXPECT_NEAR(result, 0.5, 1e-9);
}

// Test: no trades → OFI = 0
TEST_F(OFITest, NoTradesReturnsZero) {
    double result = ofi.compute(book, trades);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test: OFI is within [-1, +1] range
TEST_F(OFITest, ResultInRange) {
    trades.add({Symbol("AAPL"), double_to_price(100.0), 500, Side::Buy, 1000});
    trades.add({Symbol("AAPL"), double_to_price(100.0), 200, Side::Sell, 2000});

    double result = ofi.compute(book, trades);
    EXPECT_GE(result, -1.0);
    EXPECT_LE(result, 1.0);
}

// Test: name returns "ofi"
TEST_F(OFITest, NameReturnsOfi) {
    EXPECT_EQ(ofi.name(), "ofi");
}

// Test: window eviction removes old trades
TEST_F(OFITest, WindowEvictsOldTrades) {
    OrderFlowImbalance short_ofi(1000ULL);  // 1 microsecond window

    // Old trades: all buys at timestamp 100
    trades.add({Symbol("AAPL"), double_to_price(100.0), 500, Side::Buy, 100});
    // New trade: sell at timestamp 10000 (well outside the 1us window from old trades)
    trades.add({Symbol("AAPL"), double_to_price(100.0), 100, Side::Sell, 10000});

    double result = short_ofi.compute(book, trades);
    // Old buy trades should be evicted; only the sell at 10000 remains
    EXPECT_DOUBLE_EQ(result, -1.0);
}

// Test: extends FeatureBase
TEST_F(OFITest, ExtendsFeatureBase) {
    FeatureBase* base = &ofi;
    EXPECT_EQ(base->name(), "ofi");
}
