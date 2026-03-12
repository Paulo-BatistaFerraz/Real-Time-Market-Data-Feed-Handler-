#include <gtest/gtest.h>
#include "qf/simulator/sim_matching_engine.hpp"
#include "qf/core/order.hpp"

using namespace qf;
using namespace qf::simulator;

class SimMatchingEngineTest : public ::testing::Test {
protected:
    SimOrderBook book_;
    Symbol sym_{"AAPL"};
};

TEST_F(SimMatchingEngineTest, NoCrossNoTrade) {
    core::Order bid(1, sym_, Side::Buy, 10000, 100, 1000);
    core::Order ask(2, sym_, Side::Sell, 10100, 100, 1001);
    book_.add(bid);
    book_.add(ask);

    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book_.live_order_count(), 2u);
}

TEST_F(SimMatchingEngineTest, ExactCrossProducesTrade) {
    core::Order bid(1, sym_, Side::Buy, 10000, 100, 1000);
    core::Order ask(2, sym_, Side::Sell, 10000, 100, 1001);
    book_.add(bid);
    book_.add(ask);

    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, 10000u);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_EQ(trades[0].buy_order_id, 1u);
    EXPECT_EQ(trades[0].sell_order_id, 2u);
    EXPECT_EQ(book_.live_order_count(), 0u);
}

TEST_F(SimMatchingEngineTest, BidAboveAskProducesTrade) {
    core::Order bid(1, sym_, Side::Buy, 10200, 50, 1000);
    core::Order ask(2, sym_, Side::Sell, 10000, 50, 1001);
    book_.add(bid);
    book_.add(ask);

    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, 10000u);  // passive (ask) price
    EXPECT_EQ(trades[0].quantity, 50u);
}

TEST_F(SimMatchingEngineTest, PartialFillLeavesResidue) {
    core::Order bid(1, sym_, Side::Buy, 10000, 200, 1000);
    core::Order ask(2, sym_, Side::Sell, 10000, 80, 1001);
    book_.add(bid);
    book_.add(ask);

    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 80u);

    EXPECT_EQ(book_.live_order_count(), 1u);
    const auto* remaining = book_.find(1);
    ASSERT_NE(remaining, nullptr);
    EXPECT_EQ(remaining->remaining_quantity, 120u);
}

TEST_F(SimMatchingEngineTest, MultipleCrossesProduceMultipleTrades) {
    core::Order bid(1, sym_, Side::Buy, 10500, 300, 1000);
    core::Order ask1(2, sym_, Side::Sell, 10000, 100, 1001);
    core::Order ask2(3, sym_, Side::Sell, 10200, 100, 1002);
    book_.add(bid);
    book_.add(ask1);
    book_.add(ask2);

    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);

    ASSERT_GE(trades.size(), 2u);

    Quantity total_qty = 0;
    for (const auto& t : trades) {
        total_qty += t.quantity;
    }
    EXPECT_EQ(total_qty, 200u);

    const auto* remaining = book_.find(1);
    ASSERT_NE(remaining, nullptr);
    EXPECT_EQ(remaining->remaining_quantity, 100u);
}

TEST_F(SimMatchingEngineTest, EmptyBookNoTrades) {
    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);
    EXPECT_TRUE(trades.empty());
}

TEST_F(SimMatchingEngineTest, OnlyBidsNoTrades) {
    core::Order bid(1, sym_, Side::Buy, 10000, 100, 1000);
    book_.add(bid);

    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);
    EXPECT_TRUE(trades.empty());
}

TEST_F(SimMatchingEngineTest, OnlyAsksNoTrades) {
    core::Order ask(1, sym_, Side::Sell, 10000, 100, 1000);
    book_.add(ask);

    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);
    EXPECT_TRUE(trades.empty());
}

TEST_F(SimMatchingEngineTest, TradeSymbolSetCorrectly) {
    core::Order bid(1, sym_, Side::Buy, 10000, 50, 1000);
    core::Order ask(2, sym_, Side::Sell, 10000, 50, 1001);
    book_.add(bid);
    book_.add(ask);

    SimMatchingEngine engine(book_);
    auto trades = engine.try_match(sym_);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].symbol, sym_);
}
