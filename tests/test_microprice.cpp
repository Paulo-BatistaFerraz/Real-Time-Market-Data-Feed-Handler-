#include <gtest/gtest.h>
#include "qf/signals/features/microprice.hpp"
#include "qf/common/types.hpp"
#include <cmath>

using namespace qf;
using namespace qf::signals;

class MicropriceTest : public ::testing::Test {
protected:
    Microprice microprice;
    core::OrderBook book{Symbol("AAPL")};
    TradeHistory trades;
};

// Test: basic microprice computation
TEST_F(MicropriceTest, BasicComputation) {
    // bid=100 qty=200, ask=102 qty=100
    // microprice = (100*100 + 102*200) / (200+100) = (10000 + 20400) / 300 = 30400/300 = 101.333...
    book.add_order(1, Side::Buy, double_to_price(100.0), 200, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    double result = microprice.compute(book, trades);
    double expected = (100.0 * 100 + 102.0 * 200) / (200 + 100);
    EXPECT_NEAR(result, expected, 1e-9);
}

// Test: equal quantities → microprice equals simple mid
TEST_F(MicropriceTest, EqualQuantitiesEqualsMid) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 500, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 500, 0);

    double result = microprice.compute(book, trades);
    // (100*500 + 102*500) / (500+500) = 101.0
    EXPECT_DOUBLE_EQ(result, 101.0);
}

// Test: heavy bid side → microprice closer to ask
TEST_F(MicropriceTest, HeavyBidCloserToAsk) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 900, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    double result = microprice.compute(book, trades);
    // microprice = (100*100 + 102*900) / 1000 = (10000 + 91800)/1000 = 101.8
    EXPECT_NEAR(result, 101.8, 1e-9);
}

// Test: no bid returns NaN
TEST_F(MicropriceTest, NoBidReturnsNaN) {
    book.add_order(1, Side::Sell, double_to_price(102.0), 100, 0);
    double result = microprice.compute(book, trades);
    EXPECT_TRUE(std::isnan(result));
}

// Test: no ask returns NaN
TEST_F(MicropriceTest, NoAskReturnsNaN) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 100, 0);
    double result = microprice.compute(book, trades);
    EXPECT_TRUE(std::isnan(result));
}

// Test: empty book returns NaN
TEST_F(MicropriceTest, EmptyBookReturnsNaN) {
    double result = microprice.compute(book, trades);
    EXPECT_TRUE(std::isnan(result));
}

// Test: name returns "microprice"
TEST_F(MicropriceTest, NameReturnsMicroprice) {
    EXPECT_EQ(microprice.name(), "microprice");
}

// Test: extends FeatureBase
TEST_F(MicropriceTest, ExtendsFeatureBase) {
    FeatureBase* base = &microprice;
    EXPECT_EQ(base->name(), "microprice");
}
