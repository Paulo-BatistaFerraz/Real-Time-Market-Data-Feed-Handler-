#include <gtest/gtest.h>
#include "qf/oms/order_manager.hpp"
#include "qf/strategy/strategy_types.hpp"

using namespace qf;
using namespace qf::oms;

static strategy::Decision make_decision(const char* sym, Side side, Quantity qty,
                                        strategy::OrderType type = strategy::OrderType::Market,
                                        Price limit = 0) {
    strategy::Decision d;
    d.symbol = Symbol(sym);
    d.side = side;
    d.quantity = qty;
    d.order_type = type;
    d.limit_price = limit;
    d.urgency = strategy::Urgency::Medium;
    return d;
}

// --- Submit ---

TEST(OrderManager, SubmitCreatesOrderInSentState) {
    OrderManager mgr;
    auto result = mgr.submit(make_decision("AAPL", Side::Buy, 100));

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.order_id, 0u);

    auto order = mgr.get_order(result.order_id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->state, OrderState::Sent);
    EXPECT_EQ(order->quantity, 100u);
    EXPECT_EQ(order->side, Side::Buy);
}

TEST(OrderManager, SubmitZeroQuantityFails) {
    OrderManager mgr;
    auto result = mgr.submit(make_decision("AAPL", Side::Buy, 0));
    EXPECT_FALSE(result.success);
}

// --- Full lifecycle: New -> Sent -> Acked -> Filled ---

TEST(OrderManager, FullLifecycleNewSentAckedFilled) {
    OrderManager mgr;

    // Submit → New → Sent
    auto sub = mgr.submit(make_decision("MSFT", Side::Sell, 50));
    ASSERT_TRUE(sub.success);
    OrderId id = sub.order_id;

    auto order = mgr.get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->state, OrderState::Sent);

    // Ack → Sent → Acked
    auto ack = mgr.on_ack(id);
    ASSERT_TRUE(ack.success);
    order = mgr.get_order(id);
    EXPECT_EQ(order->state, OrderState::Acked);

    // Fill → Acked → Filled
    FillReport fill;
    fill.order_id = id;
    fill.fill_price = 150'0000;
    fill.fill_quantity = 50;
    fill.is_final = true;

    auto fr = mgr.on_fill(fill);
    ASSERT_TRUE(fr.success);
    order = mgr.get_order(id);
    EXPECT_EQ(order->state, OrderState::Filled);
    EXPECT_TRUE(order->is_terminal());
    EXPECT_EQ(order->filled_quantity, 50u);
}

// --- Partial fills ---

TEST(OrderManager, PartialFillThenFilled) {
    OrderManager mgr;
    auto sub = mgr.submit(make_decision("GOOG", Side::Buy, 100));
    ASSERT_TRUE(sub.success);
    OrderId id = sub.order_id;

    mgr.on_ack(id);

    // Partial fill: 60 of 100
    FillReport partial;
    partial.order_id = id;
    partial.fill_price = 200'0000;
    partial.fill_quantity = 60;
    partial.is_final = false;

    auto r1 = mgr.on_fill(partial);
    ASSERT_TRUE(r1.success);
    auto order = mgr.get_order(id);
    EXPECT_EQ(order->state, OrderState::PartialFill);
    EXPECT_EQ(order->filled_quantity, 60u);

    // Final fill: remaining 40
    FillReport final_fill;
    final_fill.order_id = id;
    final_fill.fill_price = 200'0000;
    final_fill.fill_quantity = 40;
    final_fill.is_final = true;

    auto r2 = mgr.on_fill(final_fill);
    ASSERT_TRUE(r2.success);
    order = mgr.get_order(id);
    EXPECT_EQ(order->state, OrderState::Filled);
    EXPECT_EQ(order->filled_quantity, 100u);
    EXPECT_EQ(order->remaining(), 0u);
}

// --- Invalid transition: New -> Filled directly is rejected ---

TEST(OrderManager, InvalidTransitionNewToFilledRejected) {
    OrderManager mgr;
    auto sub = mgr.submit(make_decision("TSLA", Side::Buy, 10));
    ASSERT_TRUE(sub.success);
    OrderId id = sub.order_id;

    // Order is in Sent state; trying to fill without ack first
    // Sent -> Filled is not valid in the transition table
    FillReport fill;
    fill.order_id = id;
    fill.fill_price = 100'0000;
    fill.fill_quantity = 10;
    fill.is_final = true;

    auto result = mgr.on_fill(fill);
    // The transition Sent -> Filled is invalid
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());

    // Order state should remain Sent
    auto order = mgr.get_order(id);
    EXPECT_EQ(order->state, OrderState::Sent);
}

// --- Cancel ---

TEST(OrderManager, CancelAckedOrder) {
    OrderManager mgr;
    auto sub = mgr.submit(make_decision("AMZN", Side::Sell, 25));
    ASSERT_TRUE(sub.success);
    mgr.on_ack(sub.order_id);

    auto cancel = mgr.cancel(sub.order_id);
    EXPECT_TRUE(cancel.success);

    auto order = mgr.get_order(sub.order_id);
    EXPECT_EQ(order->state, OrderState::Cancelled);
    EXPECT_TRUE(order->is_terminal());
}

// --- Reject ---

TEST(OrderManager, RejectSentOrder) {
    OrderManager mgr;
    auto sub = mgr.submit(make_decision("META", Side::Buy, 10));
    ASSERT_TRUE(sub.success);

    auto rej = mgr.on_reject(sub.order_id, "insufficient margin");
    EXPECT_TRUE(rej.success);

    auto order = mgr.get_order(sub.order_id);
    EXPECT_EQ(order->state, OrderState::Rejected);
}

// --- Journal records all transitions ---

TEST(OrderManager, JournalRecordsAllTransitions) {
    OrderManager mgr;
    auto sub = mgr.submit(make_decision("NVDA", Side::Buy, 100));
    mgr.on_ack(sub.order_id);

    FillReport fill;
    fill.order_id = sub.order_id;
    fill.fill_price = 500'0000;
    fill.fill_quantity = 100;
    fill.is_final = true;
    mgr.on_fill(fill);

    // Journal should have: New->Sent, Sent->Acked, Acked->Filled
    auto history = mgr.journal().get_history(sub.order_id);
    ASSERT_EQ(history.size(), 3u);
    EXPECT_EQ(history[0].event.to_state, OrderState::Sent);
    EXPECT_EQ(history[1].event.to_state, OrderState::Acked);
    EXPECT_EQ(history[2].event.to_state, OrderState::Filled);
}

// --- Unknown order ---

TEST(OrderManager, AckUnknownOrderFails) {
    OrderManager mgr;
    auto result = mgr.on_ack(999);
    EXPECT_FALSE(result.success);
}

TEST(OrderManager, UniqueOrderIds) {
    OrderManager mgr;
    auto r1 = mgr.submit(make_decision("AAA", Side::Buy, 1));
    auto r2 = mgr.submit(make_decision("BBB", Side::Sell, 1));
    EXPECT_NE(r1.order_id, r2.order_id);
}
