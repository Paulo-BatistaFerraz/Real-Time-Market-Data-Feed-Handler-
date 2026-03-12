#include <gtest/gtest.h>
#include "qf/strategy/position/pnl_calculator.hpp"
#include "qf/common/types.hpp"
#include <cmath>

using namespace qf;
using namespace qf::strategy;

class PnLCalculatorTest : public ::testing::Test {
protected:
    PnLCalculator calc;
    Symbol sym{"AAPL"};

    Fill make_fill(Side side, Price price, Quantity qty, uint64_t ts = 0) {
        Fill f;
        f.timestamp = ts;
        f.symbol = sym;
        f.side = side;
        f.price = price;
        f.quantity = qty;
        return f;
    }
};

// Test: no fills → zero PnL
TEST_F(PnLCalculatorTest, NoFillsZeroPnl) {
    EXPECT_DOUBLE_EQ(calc.realized_pnl(sym), 0.0);
    EXPECT_DOUBLE_EQ(calc.unrealized_pnl(sym), 0.0);
    EXPECT_DOUBLE_EQ(calc.total_pnl(), 0.0);
}

// Test: buy only → no realized PnL (open position)
TEST_F(PnLCalculatorTest, BuyOnlyNoRealizedPnl) {
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    EXPECT_DOUBLE_EQ(calc.realized_pnl(sym), 0.0);
}

// Test: long round-trip with profit
TEST_F(PnLCalculatorTest, LongRoundTripProfit) {
    // Buy 100 at $100, sell 100 at $105 → PnL = +$5 * 100 = +$500
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Sell, double_to_price(105.0), 100));

    EXPECT_NEAR(calc.realized_pnl(sym), 500.0, 0.01);
}

// Test: long round-trip with loss
TEST_F(PnLCalculatorTest, LongRoundTripLoss) {
    // Buy 100 at $100, sell 100 at $95 → PnL = -$5 * 100 = -$500
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Sell, double_to_price(95.0), 100));

    EXPECT_NEAR(calc.realized_pnl(sym), -500.0, 0.01);
}

// Test: short round-trip with profit
TEST_F(PnLCalculatorTest, ShortRoundTripProfit) {
    // Sell 100 at $100, buy back at $95 → PnL = +$5 * 100 = +$500
    calc.on_fill(make_fill(Side::Sell, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Buy, double_to_price(95.0), 100));

    EXPECT_NEAR(calc.realized_pnl(sym), 500.0, 0.01);
}

// Test: short round-trip with loss
TEST_F(PnLCalculatorTest, ShortRoundTripLoss) {
    // Sell 100 at $100, buy back at $105 → PnL = -$5 * 100 = -$500
    calc.on_fill(make_fill(Side::Sell, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Buy, double_to_price(105.0), 100));

    EXPECT_NEAR(calc.realized_pnl(sym), -500.0, 0.01);
}

// Test: partial close — FIFO accounting
TEST_F(PnLCalculatorTest, PartialCloseFIFO) {
    // Buy 100 at $100, sell 50 at $110 → realized = $10 * 50 = $500
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Sell, double_to_price(110.0), 50));

    EXPECT_NEAR(calc.realized_pnl(sym), 500.0, 0.01);
}

// Test: multiple lots FIFO order
TEST_F(PnLCalculatorTest, MultipleLotsFIFO) {
    // Buy 50 at $100, buy 50 at $110
    // Sell 50 → should close the $100 lot first (FIFO)
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 50));
    calc.on_fill(make_fill(Side::Buy, double_to_price(110.0), 50));
    calc.on_fill(make_fill(Side::Sell, double_to_price(105.0), 50));

    // PnL = (105 - 100) * 50 = $250 (first lot matched)
    EXPECT_NEAR(calc.realized_pnl(sym), 250.0, 0.01);
}

// Test: mark to market computes unrealized PnL
TEST_F(PnLCalculatorTest, MarkToMarket) {
    // Buy 100 at $100, mark at $110 → unrealized = $10 * 100 = $1000
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    calc.mark_to_market(sym, double_to_price(110.0));

    EXPECT_NEAR(calc.unrealized_pnl(sym), 1000.0, 0.01);
}

// Test: mark to market for short position
TEST_F(PnLCalculatorTest, MarkToMarketShort) {
    // Sell 100 at $100, mark at $95 → unrealized = $5 * 100 = $500
    calc.on_fill(make_fill(Side::Sell, double_to_price(100.0), 100));
    calc.mark_to_market(sym, double_to_price(95.0));

    EXPECT_NEAR(calc.unrealized_pnl(sym), 500.0, 0.01);
}

// Test: total PnL = realized + unrealized
TEST_F(PnLCalculatorTest, TotalPnlIsSum) {
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Sell, double_to_price(110.0), 50));
    // Realized: (110-100)*50 = 500
    // Remaining 50 at $100, mark at $105
    calc.mark_to_market(sym, double_to_price(105.0));
    // Unrealized: (105-100)*50 = 250

    EXPECT_NEAR(calc.total_realized_pnl(), 500.0, 0.01);
    EXPECT_NEAR(calc.total_unrealized_pnl(), 250.0, 0.01);
    EXPECT_NEAR(calc.total_pnl(), 750.0, 0.01);
}

// Test: multiple symbols
TEST_F(PnLCalculatorTest, MultipleSymbols) {
    Symbol sym2{"MSFT"};

    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Sell, double_to_price(110.0), 100));
    // AAPL realized: +$1000

    Fill f2;
    f2.timestamp = 0;
    f2.symbol = sym2;
    f2.side = Side::Buy;
    f2.price = double_to_price(50.0);
    f2.quantity = 200;
    calc.on_fill(f2);

    Fill f3;
    f3.timestamp = 0;
    f3.symbol = sym2;
    f3.side = Side::Sell;
    f3.price = double_to_price(55.0);
    f3.quantity = 200;
    calc.on_fill(f3);
    // MSFT realized: +$5 * 200 = +$1000

    EXPECT_NEAR(calc.realized_pnl(sym), 1000.0, 0.01);
    EXPECT_NEAR(calc.realized_pnl(sym2), 1000.0, 0.01);
    EXPECT_NEAR(calc.total_realized_pnl(), 2000.0, 0.01);
}

// Test: reset clears all state
TEST_F(PnLCalculatorTest, ResetClearsAll) {
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Sell, double_to_price(110.0), 100));
    calc.reset();

    EXPECT_DOUBLE_EQ(calc.realized_pnl(sym), 0.0);
    EXPECT_DOUBLE_EQ(calc.total_pnl(), 0.0);
}

// Test: sell then buy reversal (flip from long to short)
TEST_F(PnLCalculatorTest, PositionReversal) {
    // Buy 100 at $100, then sell 200 at $110
    // First 100 closes long: realized = (110-100)*100 = $1000
    // Remaining 100 opens short lot at $110
    calc.on_fill(make_fill(Side::Buy, double_to_price(100.0), 100));
    calc.on_fill(make_fill(Side::Sell, double_to_price(110.0), 200));

    EXPECT_NEAR(calc.realized_pnl(sym), 1000.0, 0.01);

    // Now buy back 100 at $105 → closes short: realized += (110-105)*100 = $500
    calc.on_fill(make_fill(Side::Buy, double_to_price(105.0), 100));
    EXPECT_NEAR(calc.realized_pnl(sym), 1500.0, 0.01);
}
