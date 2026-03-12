#include <gtest/gtest.h>
#include "qf/core/order_book.hpp"

using namespace qf;
using namespace qf::core;

// --- Empty book ---

TEST(OrderBook, EmptyBookBestBidReturnsNullopt) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBook, EmptyBookBestAskReturnsNullopt) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBook, EmptyBookSpreadIsZero) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_DOUBLE_EQ(book.spread(), 0.0);
}

TEST(OrderBook, EmptyBookOrderCountIsZero) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_EQ(book.order_count(), 0u);
}

// --- Add orders on both sides ---

TEST(OrderBook, AddBidOrderSetsBestBid) {
    OrderBook book(Symbol("AAPL"));
    auto update = book.add_order(1, Side::Buy, 10000, 100, 1000);
    ASSERT_TRUE(update.has_value());
    EXPECT_EQ(book.best_bid().value(), 10000u);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBook, AddAskOrderSetsBestAsk) {
    OrderBook book(Symbol("AAPL"));
    auto update = book.add_order(1, Side::Sell, 10100, 50, 1000);
    ASSERT_TRUE(update.has_value());
    EXPECT_EQ(book.best_ask().value(), 10100u);
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBook, AddBothSidesBestBidAskCorrect) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 10050, 200, 1001);
    book.add_order(3, Side::Sell, 10100, 150, 1002);
    book.add_order(4, Side::Sell, 10200, 75, 1003);

    // Best bid is highest buy price
    EXPECT_EQ(book.best_bid().value(), 10050u);
    // Best ask is lowest sell price
    EXPECT_EQ(book.best_ask().value(), 10100u);
}

TEST(OrderBook, AddMultipleBidsHighestIsBest) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 9900, 100, 1000);
    book.add_order(2, Side::Buy, 10100, 100, 1001);
    book.add_order(3, Side::Buy, 10000, 100, 1002);
    EXPECT_EQ(book.best_bid().value(), 10100u);
}

TEST(OrderBook, AddMultipleAsksLowestIsBest) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Sell, 10300, 100, 1000);
    book.add_order(2, Side::Sell, 10100, 100, 1001);
    book.add_order(3, Side::Sell, 10200, 100, 1002);
    EXPECT_EQ(book.best_ask().value(), 10100u);
}

TEST(OrderBook, AddDuplicateOrderIdReturnsNullopt) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_TRUE(book.add_order(1, Side::Buy, 10000, 100, 1000).has_value());
    EXPECT_FALSE(book.add_order(1, Side::Buy, 10050, 200, 1001).has_value());
    EXPECT_EQ(book.order_count(), 1u);
}

TEST(OrderBook, AddOrderReturnsBookUpdate) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    auto update = book.add_order(2, Side::Sell, 10100, 50, 2000);
    ASSERT_TRUE(update.has_value());
    EXPECT_EQ(update->best_bid.value(), 10000u);
    EXPECT_EQ(update->best_ask.value(), 10100u);
    EXPECT_EQ(update->timestamp, 2000u);
}

TEST(OrderBook, SpreadCalculation) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);   // $1.0000
    book.add_order(2, Side::Sell, 10100, 100, 1001);   // $1.0100
    EXPECT_NEAR(book.spread(), 0.01, 1e-9);
}

// --- Cancel order ---

TEST(OrderBook, CancelOrderDecreaseQuantity) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 10000, 200, 1001);

    auto update = book.cancel_order(1);
    ASSERT_TRUE(update.has_value());
    // Level still exists with order 2's quantity
    EXPECT_EQ(book.best_bid_qty(), 200u);
    EXPECT_EQ(book.order_count(), 1u);
}

TEST(OrderBook, CancelLastOrderRemovesLevel) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 9900, 50, 1001);

    book.cancel_order(1);
    // Best bid should now be the lower price
    EXPECT_EQ(book.best_bid().value(), 9900u);
    EXPECT_EQ(book.best_bid_qty(), 50u);
}

TEST(OrderBook, CancelOnlyOrderMakesSideEmpty) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Sell, 10100, 100, 1000);
    book.cancel_order(1);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBook, CancelNonExistentReturnsNullopt) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_FALSE(book.cancel_order(999).has_value());
}

// --- Execute order ---

TEST(OrderBook, PartialFillReducesQuantity) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);

    auto update = book.execute_order(1, 40);
    ASSERT_TRUE(update.has_value());
    EXPECT_EQ(book.best_bid_qty(), 60u);
    EXPECT_EQ(book.order_count(), 1u);
    EXPECT_EQ(book.best_bid().value(), 10000u);
}

TEST(OrderBook, FullFillRemovesOrder) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Sell, 10100, 100, 1000);

    auto update = book.execute_order(1, 100);
    ASSERT_TRUE(update.has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.order_count(), 0u);
}

TEST(OrderBook, FullFillRemovesEmptyLevel) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 9900, 50, 1001);

    book.execute_order(1, 100);
    // Level at 10000 should be gone; best bid moves to 9900
    EXPECT_EQ(book.best_bid().value(), 9900u);
}

TEST(OrderBook, FullFillWithMultipleOrdersOnLevel) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 10000, 200, 1001);

    book.execute_order(1, 100);
    // Order 2 still on the level
    EXPECT_EQ(book.best_bid().value(), 10000u);
    EXPECT_EQ(book.best_bid_qty(), 200u);
    EXPECT_EQ(book.order_count(), 1u);
}

TEST(OrderBook, ExecuteZeroQuantityReturnsNullopt) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    EXPECT_FALSE(book.execute_order(1, 0).has_value());
}

TEST(OrderBook, ExecuteExceedingQuantityReturnsNullopt) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    EXPECT_FALSE(book.execute_order(1, 101).has_value());
    // Order should be unchanged
    EXPECT_EQ(book.best_bid_qty(), 100u);
}

TEST(OrderBook, ExecuteNonExistentReturnsNullopt) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_FALSE(book.execute_order(999, 50).has_value());
}

// --- Replace order ---

TEST(OrderBook, ReplaceUpdatesOldAndNewLevel) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 10000, 150, 1001);

    auto update = book.replace_order(1, 10100, 200);
    ASSERT_TRUE(update.has_value());

    // Old level should have only order 2's quantity
    auto depth = book.bid_depth(5);
    ASSERT_EQ(depth.size(), 2u);
    // First level: 10100 (highest bid, the replaced order)
    EXPECT_EQ(depth[0].price, 10100u);
    EXPECT_EQ(depth[0].quantity, 200u);
    EXPECT_EQ(depth[0].order_count, 1u);
    // Second level: 10000 (order 2 remains)
    EXPECT_EQ(depth[1].price, 10000u);
    EXPECT_EQ(depth[1].quantity, 150u);
    EXPECT_EQ(depth[1].order_count, 1u);
}

TEST(OrderBook, ReplaceSoleLevelCreatesNewRemovesOld) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Sell, 10100, 100, 1000);

    book.replace_order(1, 10200, 75);
    EXPECT_EQ(book.best_ask().value(), 10200u);
    EXPECT_EQ(book.best_ask_qty(), 75u);

    // Old level should be gone — only one level in the book
    auto depth = book.ask_depth(5);
    ASSERT_EQ(depth.size(), 1u);
    EXPECT_EQ(depth[0].price, 10200u);
}

TEST(OrderBook, ReplaceNonExistentReturnsNullopt) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_FALSE(book.replace_order(999, 10000, 100).has_value());
}

TEST(OrderBook, ReplaceSamePriceUpdatesQuantity) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);

    book.replace_order(1, 10000, 250);
    EXPECT_EQ(book.best_bid_qty(), 250u);
}

// --- Multiple symbols don't interfere ---

TEST(OrderBook, MultipleSymbolsIndependent) {
    OrderBook aapl(Symbol("AAPL"));
    OrderBook msft(Symbol("MSFT"));

    aapl.add_order(1, Side::Buy, 15000, 100, 1000);
    aapl.add_order(2, Side::Sell, 15100, 50, 1001);

    msft.add_order(3, Side::Buy, 30000, 200, 1002);
    msft.add_order(4, Side::Sell, 30200, 75, 1003);

    // AAPL values
    EXPECT_EQ(aapl.best_bid().value(), 15000u);
    EXPECT_EQ(aapl.best_ask().value(), 15100u);
    EXPECT_EQ(aapl.order_count(), 2u);

    // MSFT values — completely independent
    EXPECT_EQ(msft.best_bid().value(), 30000u);
    EXPECT_EQ(msft.best_ask().value(), 30200u);
    EXPECT_EQ(msft.order_count(), 2u);

    // Cancel in AAPL doesn't affect MSFT
    aapl.cancel_order(1);
    EXPECT_FALSE(aapl.best_bid().has_value());
    EXPECT_EQ(msft.best_bid().value(), 30000u);
}

TEST(OrderBook, SymbolStoredCorrectly) {
    OrderBook book(Symbol("GOOG"));
    EXPECT_EQ(book.symbol(), Symbol("GOOG"));
}

// --- top_n_levels returns correct sorted levels ---

TEST(OrderBook, TopNLevelsBidsSortedDescending) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 9800, 50, 1000);
    book.add_order(2, Side::Buy, 10000, 100, 1001);
    book.add_order(3, Side::Buy, 9900, 75, 1002);

    auto levels = book.top_n_levels(Side::Buy, 3);
    ASSERT_EQ(levels.size(), 3u);
    // Highest price first
    EXPECT_EQ(levels[0].price, 10000u);
    EXPECT_EQ(levels[0].quantity, 100u);
    EXPECT_EQ(levels[1].price, 9900u);
    EXPECT_EQ(levels[1].quantity, 75u);
    EXPECT_EQ(levels[2].price, 9800u);
    EXPECT_EQ(levels[2].quantity, 50u);
}

TEST(OrderBook, TopNLevelsAsksSortedAscending) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Sell, 10200, 60, 1000);
    book.add_order(2, Side::Sell, 10100, 80, 1001);
    book.add_order(3, Side::Sell, 10300, 40, 1002);

    auto levels = book.top_n_levels(Side::Sell, 3);
    ASSERT_EQ(levels.size(), 3u);
    // Lowest price first
    EXPECT_EQ(levels[0].price, 10100u);
    EXPECT_EQ(levels[0].quantity, 80u);
    EXPECT_EQ(levels[1].price, 10200u);
    EXPECT_EQ(levels[1].quantity, 60u);
    EXPECT_EQ(levels[2].price, 10300u);
    EXPECT_EQ(levels[2].quantity, 40u);
}

TEST(OrderBook, TopNLevelsReturnsFewerIfNotEnough) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);

    auto levels = book.top_n_levels(Side::Buy, 5);
    ASSERT_EQ(levels.size(), 1u);
    EXPECT_EQ(levels[0].price, 10000u);
}

TEST(OrderBook, TopNLevelsEmptySideReturnsEmpty) {
    OrderBook book(Symbol("AAPL"));
    auto levels = book.top_n_levels(Side::Sell, 5);
    EXPECT_TRUE(levels.empty());
}

TEST(OrderBook, TopNLevelsAggregatesMultipleOrdersAtSamePrice) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 10000, 200, 1001);
    book.add_order(3, Side::Buy, 9900, 50, 1002);

    auto levels = book.top_n_levels(Side::Buy, 2);
    ASSERT_EQ(levels.size(), 2u);
    EXPECT_EQ(levels[0].price, 10000u);
    EXPECT_EQ(levels[0].quantity, 300u);
    EXPECT_EQ(levels[0].order_count, 2u);
    EXPECT_EQ(levels[1].price, 9900u);
    EXPECT_EQ(levels[1].quantity, 50u);
    EXPECT_EQ(levels[1].order_count, 1u);
}

// --- Bid/Ask depth convenience methods ---

TEST(OrderBook, BidDepthMatchesTopNBuy) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 9900, 50, 1001);

    auto depth = book.bid_depth(2);
    auto top = book.top_n_levels(Side::Buy, 2);
    ASSERT_EQ(depth.size(), top.size());
    for (size_t i = 0; i < depth.size(); ++i) {
        EXPECT_EQ(depth[i].price, top[i].price);
        EXPECT_EQ(depth[i].quantity, top[i].quantity);
    }
}

TEST(OrderBook, AskDepthMatchesTopNSell) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Sell, 10100, 100, 1000);
    book.add_order(2, Side::Sell, 10200, 50, 1001);

    auto depth = book.ask_depth(2);
    auto top = book.top_n_levels(Side::Sell, 2);
    ASSERT_EQ(depth.size(), top.size());
    for (size_t i = 0; i < depth.size(); ++i) {
        EXPECT_EQ(depth[i].price, top[i].price);
        EXPECT_EQ(depth[i].quantity, top[i].quantity);
    }
}

// --- Best bid/ask qty ---

TEST(OrderBook, BestBidQtyAggregatesLevel) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Buy, 10000, 100, 1000);
    book.add_order(2, Side::Buy, 10000, 200, 1001);
    EXPECT_EQ(book.best_bid_qty(), 300u);
}

TEST(OrderBook, BestAskQtyAggregatesLevel) {
    OrderBook book(Symbol("AAPL"));
    book.add_order(1, Side::Sell, 10100, 50, 1000);
    book.add_order(2, Side::Sell, 10100, 75, 1001);
    EXPECT_EQ(book.best_ask_qty(), 125u);
}

TEST(OrderBook, EmptyBookBestBidQtyIsZero) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_EQ(book.best_bid_qty(), 0u);
}

TEST(OrderBook, EmptyBookBestAskQtyIsZero) {
    OrderBook book(Symbol("AAPL"));
    EXPECT_EQ(book.best_ask_qty(), 0u);
}
