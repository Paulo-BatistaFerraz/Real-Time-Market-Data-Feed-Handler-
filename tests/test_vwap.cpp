#include <gtest/gtest.h>
#include "qf/signals/features/vwap.hpp"
#include "qf/common/types.hpp"

using namespace qf;
using namespace qf::signals;

class VWAPTest : public ::testing::Test {
protected:
    VWAP vwap;
    core::OrderBook book{Symbol("AAPL")};
    TradeHistory trades;

    TradeRecord make_trade(double price, uint32_t qty) {
        TradeRecord t;
        t.symbol = Symbol("AAPL");
        t.price = double_to_price(price);
        t.quantity = qty;
        t.aggressor_side = Side::Buy;
        t.timestamp = 0;
        return t;
    }
};

// Test: single trade → VWAP equals trade price
TEST_F(VWAPTest, SingleTradeEqualsTradePrice) {
    trades.add(make_trade(100.0, 500));
    double result = vwap.compute(book, trades);
    EXPECT_DOUBLE_EQ(result, 100.0);
}

// Test: known sequence of trades → correct VWAP
TEST_F(VWAPTest, KnownSequenceCorrectVWAP) {
    // Trade 1: 100 shares @ $10.00
    // Trade 2: 200 shares @ $12.00
    // Trade 3: 300 shares @ $11.00
    // VWAP = (100*10 + 200*12 + 300*11) / (100+200+300)
    //      = (1000 + 2400 + 3300) / 600
    //      = 6700 / 600
    //      = 11.166666...
    trades.add(make_trade(10.0, 100));
    trades.add(make_trade(12.0, 200));
    trades.add(make_trade(11.0, 300));

    double result = vwap.compute(book, trades);
    EXPECT_NEAR(result, 6700.0 / 600.0, 1e-9);
}

// Test: incremental compute processes only new trades
TEST_F(VWAPTest, IncrementalCompute) {
    trades.add(make_trade(10.0, 100));
    double r1 = vwap.compute(book, trades);
    EXPECT_DOUBLE_EQ(r1, 10.0);

    trades.add(make_trade(20.0, 100));
    double r2 = vwap.compute(book, trades);
    // VWAP = (100*10 + 100*20) / 200 = 3000/200 = 15.0
    EXPECT_DOUBLE_EQ(r2, 15.0);
}

// Test: reset clears accumulators
TEST_F(VWAPTest, ResetClearsAccumulators) {
    trades.add(make_trade(50.0, 1000));
    vwap.compute(book, trades);

    vwap.reset();
    TradeHistory new_trades;
    new_trades.add(make_trade(75.0, 200));
    double result = vwap.compute(book, new_trades);
    EXPECT_DOUBLE_EQ(result, 75.0);
}

// Test: no trades returns 0
TEST_F(VWAPTest, NoTradesReturnsZero) {
    double result = vwap.compute(book, trades);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test: name returns "vwap"
TEST_F(VWAPTest, NameReturnsVwap) {
    EXPECT_EQ(vwap.name(), "vwap");
}
