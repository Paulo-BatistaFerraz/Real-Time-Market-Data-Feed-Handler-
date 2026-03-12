#include <gtest/gtest.h>
#include "qf/signals/features/twap.hpp"
#include "qf/common/types.hpp"
#include <cmath>

using namespace qf;
using namespace qf::signals;

class TWAPTest : public ::testing::Test {
protected:
    core::OrderBook book{Symbol("AAPL")};
    TradeHistory trades;

    void set_book_prices(double bid, double ask) {
        // Add orders to set best bid and ask
        static OrderId id = 1000;
        book.add_order(++id, Side::Buy, double_to_price(bid), 100, 0);
        book.add_order(++id, Side::Sell, double_to_price(ask), 100, 0);
    }

    TradeRecord make_trade(double price, uint32_t qty, uint64_t ts) {
        TradeRecord t;
        t.symbol = Symbol("AAPL");
        t.price = double_to_price(price);
        t.quantity = qty;
        t.aggressor_side = Side::Buy;
        t.timestamp = ts;
        return t;
    }
};

// Test: single sample returns that price
TEST_F(TWAPTest, SingleSampleReturnsPrice) {
    TWAP twap(5'000'000'000ULL);
    set_book_prices(99.0, 101.0);
    trades.add(make_trade(100.0, 100, 1'000'000'000ULL));

    double result = twap.compute(book, trades);
    // mid = (99+101)/2 = 100
    EXPECT_DOUBLE_EQ(result, 100.0);
}

// Test: time-weighted average with different durations
TEST_F(TWAPTest, TimeWeightedAverage) {
    TWAP twap(10'000'000'000ULL);  // 10 second window

    // First sample: mid=100, at t=1s
    set_book_prices(99.0, 101.0);
    trades.add(make_trade(100.0, 100, 1'000'000'000ULL));
    twap.compute(book, trades);

    // Cancel old orders and set new prices
    // Add new bid/ask at different price
    book.add_order(9001, Side::Buy, double_to_price(109.0), 100, 0);
    book.add_order(9002, Side::Sell, double_to_price(111.0), 100, 0);
    // Now best_bid=109, best_ask=111, mid=110
    trades.add(make_trade(110.0, 100, 3'000'000'000ULL));
    double result = twap.compute(book, trades);

    // Two samples: price=100 at t=1s, price=110 at t=3s
    // Time-weighted: 100 * (3-1)ns / (3-1)ns = only first segment
    // TWAP = 100 * 2s / 2s = 100
    EXPECT_DOUBLE_EQ(result, 100.0);
}

// Test: empty book returns NaN
TEST_F(TWAPTest, EmptyBookReturnsNaN) {
    TWAP twap(5'000'000'000ULL);
    double result = twap.compute(book, trades);
    EXPECT_TRUE(std::isnan(result));
}

// Test: name returns "twap"
TEST_F(TWAPTest, NameReturnsTwap) {
    TWAP twap;
    EXPECT_EQ(twap.name(), "twap");
}

// Test: samples outside window are evicted
TEST_F(TWAPTest, OldSamplesEvicted) {
    TWAP twap(2'000'000'000ULL);  // 2 second window

    set_book_prices(99.0, 101.0);  // mid = 100
    trades.add(make_trade(100.0, 100, 1'000'000'000ULL));
    twap.compute(book, trades);

    // Add at t=2s — still within window of first sample
    trades.add(make_trade(100.0, 100, 2'000'000'000ULL));
    twap.compute(book, trades);

    // Move far forward — t=10s, window=2s, so cutoff=8s
    // Both old samples (t=1s, t=2s) should be evicted
    book.add_order(8001, Side::Buy, double_to_price(149.0), 100, 0);
    book.add_order(8002, Side::Sell, double_to_price(151.0), 100, 0);
    // best_bid=149 (higher than 99), best_ask=111 (lower than 151)
    // Actually, map ordering: bids use greater<Price>, so best_bid = max(99, 109, 149)=149
    // asks use default (less<Price>), so best_ask = min(101, 111, 151)=101
    // mid = (149+101)/2 = 125
    trades.add(make_trade(125.0, 100, 10'000'000'000ULL));
    double result = twap.compute(book, trades);

    // Only the new sample at t=10s should remain → returns that single price
    EXPECT_DOUBLE_EQ(result, 125.0);
}

// Test: extends FeatureBase
TEST_F(TWAPTest, ExtendsFeatureBase) {
    TWAP twap;
    FeatureBase* base = &twap;
    EXPECT_EQ(base->name(), "twap");
}
