#include <gtest/gtest.h>
#include "qf/core/order_store.hpp"

using namespace qf;
using namespace qf::core;

static Order make_order(OrderId id, Price price = 10000, Quantity qty = 100) {
    return Order(id, Symbol("AAPL"), Side::Buy, price, qty, 1000);
}

TEST(OrderStore, ConstructorReserves) {
    OrderStore store(512);
    EXPECT_TRUE(store.empty());
    EXPECT_EQ(store.size(), 0u);
}

TEST(OrderStore, InsertAndFind) {
    OrderStore store;
    Order o = make_order(1);
    EXPECT_TRUE(store.add(o));
    EXPECT_EQ(store.size(), 1u);
    EXPECT_FALSE(store.empty());

    Order* found = store.find(1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->order_id, 1u);
    EXPECT_EQ(found->price, 10000u);
    EXPECT_EQ(found->remaining_quantity, 100u);
}

TEST(OrderStore, InsertDuplicateReturnsFalse) {
    OrderStore store;
    Order o = make_order(42);
    EXPECT_TRUE(store.add(o));
    EXPECT_FALSE(store.add(o));
    EXPECT_EQ(store.size(), 1u);
}

TEST(OrderStore, FindNonExistent) {
    OrderStore store;
    EXPECT_EQ(store.find(999), nullptr);

    const auto& cstore = store;
    EXPECT_EQ(cstore.find(999), nullptr);
}

TEST(OrderStore, ConstFind) {
    OrderStore store;
    store.add(make_order(5));
    const auto& cstore = store;
    const Order* found = cstore.find(5);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->order_id, 5u);
}

TEST(OrderStore, RemoveOrder) {
    OrderStore store;
    store.add(make_order(10, 5000, 200));

    auto removed = store.remove(10);
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(removed->order_id, 10u);
    EXPECT_EQ(removed->price, 5000u);
    EXPECT_EQ(removed->remaining_quantity, 200u);
    EXPECT_TRUE(store.empty());
}

TEST(OrderStore, RemoveNonExistent) {
    OrderStore store;
    auto removed = store.remove(999);
    EXPECT_FALSE(removed.has_value());
}

TEST(OrderStore, UpdatePriceQty) {
    OrderStore store;
    store.add(make_order(1, 10000, 100));

    EXPECT_TRUE(store.update_price_qty(1, 20000, 50));
    Order* o = store.find(1);
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->price, 20000u);
    EXPECT_EQ(o->remaining_quantity, 50u);
}

TEST(OrderStore, UpdateNonExistent) {
    OrderStore store;
    EXPECT_FALSE(store.update_price_qty(999, 10000, 100));
}

TEST(OrderStore, ReduceQuantity) {
    OrderStore store;
    store.add(make_order(1, 10000, 100));

    EXPECT_TRUE(store.reduce_quantity(1, 30));
    EXPECT_EQ(store.find(1)->remaining_quantity, 70u);

    EXPECT_TRUE(store.reduce_quantity(1, 70));
    EXPECT_EQ(store.find(1)->remaining_quantity, 0u);
}

TEST(OrderStore, ReduceQuantityExceedsRemaining) {
    OrderStore store;
    store.add(make_order(1, 10000, 50));
    EXPECT_FALSE(store.reduce_quantity(1, 51));
    EXPECT_EQ(store.find(1)->remaining_quantity, 50u);
}

TEST(OrderStore, ReduceQuantityNonExistent) {
    OrderStore store;
    EXPECT_FALSE(store.reduce_quantity(999, 10));
}

TEST(OrderStore, ReserveDoesNotChangeSize) {
    OrderStore store;
    store.add(make_order(1));
    store.reserve(10000);
    EXPECT_EQ(store.size(), 1u);
    EXPECT_NE(store.find(1), nullptr);
}

TEST(OrderStore, Clear) {
    OrderStore store;
    store.add(make_order(1));
    store.add(make_order(2));
    EXPECT_EQ(store.size(), 2u);
    store.clear();
    EXPECT_TRUE(store.empty());
    EXPECT_EQ(store.find(1), nullptr);
}

TEST(OrderStore, MultipleOrders) {
    OrderStore store;
    for (OrderId i = 1; i <= 1000; ++i) {
        EXPECT_TRUE(store.add(make_order(i, static_cast<Price>(i * 100), static_cast<Quantity>(i))));
    }
    EXPECT_EQ(store.size(), 1000u);

    for (OrderId i = 1; i <= 1000; ++i) {
        Order* o = store.find(i);
        ASSERT_NE(o, nullptr);
        EXPECT_EQ(o->price, static_cast<Price>(i * 100));
    }
}
