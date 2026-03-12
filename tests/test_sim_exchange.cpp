#include <gtest/gtest.h>
#include "qf/oms/sim_exchange/sim_exchange_connector.hpp"
#include "qf/oms/sim_exchange/sim_exchange.hpp"
#include "qf/oms/order_manager.hpp"
#include "qf/oms/fill_manager.hpp"
#include "qf/oms/order_router.hpp"
#include "qf/strategy/position/portfolio.hpp"

using namespace qf;
using namespace qf::oms;

// --- SimExchange fills market order at best price ---

TEST(SimExchange, MarketOrderFillsAtBestPrice) {
    std::vector<FillReport> fills;
    FillModelConfig cfg;
    cfg.reject_rate = 0.0;
    cfg.partial_fill_prob = 0.0;  // single fill

    SimExchange exchange([&](const FillReport& r) { fills.push_back(r); }, cfg);

    // Set market prices: bid=100.00, ask=100.50
    Symbol sym("AAPL");
    exchange.on_market_update(sym, 100'0000, 100'5000);

    // Buy market order should fill at ask (100.50)
    OmsOrder buy;
    buy.order_id = 1;
    buy.symbol = sym;
    buy.side = Side::Buy;
    buy.type = OrderType::Market;
    buy.quantity = 50;
    buy.filled_quantity = 0;
    buy.state = OrderState::Sent;

    exchange.submit_order(buy);

    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].order_id, 1u);
    EXPECT_EQ(fills[0].fill_price, 100'5000u);  // filled at ask
    EXPECT_EQ(fills[0].fill_quantity, 50u);
    EXPECT_TRUE(fills[0].is_final);
}

TEST(SimExchange, SellMarketOrderFillsAtBid) {
    std::vector<FillReport> fills;
    FillModelConfig cfg;
    cfg.reject_rate = 0.0;
    cfg.partial_fill_prob = 0.0;

    SimExchange exchange([&](const FillReport& r) { fills.push_back(r); }, cfg);

    Symbol sym("MSFT");
    exchange.on_market_update(sym, 200'0000, 200'5000);

    OmsOrder sell;
    sell.order_id = 2;
    sell.symbol = sym;
    sell.side = Side::Sell;
    sell.type = OrderType::Market;
    sell.quantity = 30;
    sell.filled_quantity = 0;
    sell.state = OrderState::Sent;

    exchange.submit_order(sell);

    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_price, 200'0000u);  // filled at bid
    EXPECT_EQ(fills[0].fill_quantity, 30u);
}

TEST(SimExchange, MarketOrderWithNoQuotesIsRejected) {
    std::vector<FillReport> fills;
    FillModelConfig cfg;
    cfg.reject_rate = 0.0;

    SimExchange exchange([&](const FillReport& r) { fills.push_back(r); }, cfg);

    OmsOrder order;
    order.order_id = 3;
    order.symbol = Symbol("NOPE");
    order.side = Side::Buy;
    order.type = OrderType::Market;
    order.quantity = 10;
    order.filled_quantity = 0;
    order.state = OrderState::Sent;

    exchange.submit_order(order);

    // Should get a reject (zero-qty fill)
    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_quantity, 0u);
    EXPECT_TRUE(fills[0].is_final);
}

// --- SimExchangeConnector implements ExchangeConnector ---

TEST(SimExchangeConnector, ImplementsExchangeInterface) {
    std::vector<FillReport> fills;
    FillModelConfig cfg;
    cfg.reject_rate = 0.0;
    cfg.partial_fill_prob = 0.0;

    SimExchange exchange([&](const FillReport& r) { fills.push_back(r); }, cfg);
    SimExchangeConnector connector(exchange);

    // Verify it has the right venue name
    EXPECT_EQ(connector.venue_name(), "SimExchange");

    // Verify it works through the ExchangeConnector interface
    ExchangeConnector* base = &connector;
    EXPECT_EQ(base->venue_name(), "SimExchange");

    // Set up market data so the order fills
    Symbol sym("TEST");
    exchange.on_market_update(sym, 50'0000, 51'0000);

    OmsOrder order;
    order.order_id = 10;
    order.symbol = sym;
    order.side = Side::Buy;
    order.type = OrderType::Market;
    order.quantity = 20;
    order.filled_quantity = 0;
    order.state = OrderState::Sent;

    bool sent = base->send_order(order);
    EXPECT_TRUE(sent);
    EXPECT_EQ(connector.orders_sent(), 1u);
    EXPECT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_price, 51'0000u);
}

// --- OrderRouter uses SimExchangeConnector ---

TEST(SimExchangeConnector, WorksWithOrderRouter) {
    std::vector<FillReport> fills;
    FillModelConfig cfg;
    cfg.reject_rate = 0.0;
    cfg.partial_fill_prob = 0.0;

    SimExchange exchange([&](const FillReport& r) { fills.push_back(r); }, cfg);
    auto connector = std::make_shared<SimExchangeConnector>(exchange);

    OrderRouter router;
    router.set_default_connector(connector);

    Symbol sym("ROUT");
    exchange.on_market_update(sym, 80'0000, 81'0000);

    OmsOrder order;
    order.order_id = 20;
    order.symbol = sym;
    order.side = Side::Sell;
    order.type = OrderType::Market;
    order.quantity = 15;
    order.filled_quantity = 0;
    order.state = OrderState::Sent;

    auto conf = router.route(order);
    EXPECT_TRUE(conf.routed);
    EXPECT_EQ(conf.venue, "SimExchange");
    EXPECT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].fill_price, 80'0000u);  // sell at bid
}

// --- Full integration: submit → New → Sent → Acked → Filled via callback ---

TEST(SimExchangeIntegration, SubmitThroughFullLoop) {
    OrderManager mgr;
    strategy::Portfolio portfolio;
    FillManager fill_mgr(mgr, portfolio);

    // SimExchange calls FillManager::on_fill via callback
    FillModelConfig cfg;
    cfg.reject_rate = 0.0;
    cfg.partial_fill_prob = 0.0;
    cfg.latency_ns = 0;

    SimExchange exchange(
        [&](const FillReport& report) {
            // Before processing fill, the order must be acked
            mgr.on_ack(report.order_id);
            fill_mgr.on_fill(report);
        },
        cfg
    );

    SimExchangeConnector connector(exchange);

    // Set market prices
    Symbol sym("INTG");
    exchange.on_market_update(sym, 150'0000, 151'0000);

    // Submit a decision through OrderManager
    strategy::Decision decision;
    decision.symbol = sym;
    decision.side = Side::Buy;
    decision.quantity = 100;
    decision.order_type = strategy::OrderType::Market;
    decision.limit_price = 0;
    decision.urgency = strategy::Urgency::High;

    auto sub = mgr.submit(decision);
    ASSERT_TRUE(sub.success);
    OrderId id = sub.order_id;

    // Order is in Sent state
    auto order = mgr.get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->state, OrderState::Sent);

    // Route through connector → SimExchange → fill callback → ack + fill
    OmsOrder to_send = *order;
    bool sent = connector.send_order(to_send);
    EXPECT_TRUE(sent);

    // After the callback fires, order should be Filled
    order = mgr.get_order(id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->state, OrderState::Filled);
    EXPECT_EQ(order->filled_quantity, 100u);

    // FillManager tracked it
    EXPECT_EQ(fill_mgr.total_fills(), 1u);
    EXPECT_EQ(fill_mgr.total_volume(), 100u);

    // Journal should show: New->Sent, Sent->Acked, Acked->Filled
    auto history = mgr.journal().get_history(id);
    ASSERT_EQ(history.size(), 3u);
    EXPECT_EQ(history[0].event.to_state, OrderState::Sent);
    EXPECT_EQ(history[1].event.to_state, OrderState::Acked);
    EXPECT_EQ(history[2].event.to_state, OrderState::Filled);
}

// --- Limit order fills when market reaches limit ---

TEST(SimExchange, LimitOrderFillsWhenPriceReaches) {
    std::vector<FillReport> fills;
    FillModelConfig cfg;
    cfg.reject_rate = 0.0;
    cfg.partial_fill_prob = 0.0;

    SimExchange exchange([&](const FillReport& r) { fills.push_back(r); }, cfg);

    Symbol sym("LIMT");
    exchange.on_market_update(sym, 99'0000, 101'0000);

    // Buy limit at 100.00 — ask is 101.00, won't fill yet
    OmsOrder order;
    order.order_id = 30;
    order.symbol = sym;
    order.side = Side::Buy;
    order.type = OrderType::Limit;
    order.price = 100'0000;
    order.quantity = 25;
    order.filled_quantity = 0;
    order.state = OrderState::Sent;

    exchange.submit_order(order);
    EXPECT_EQ(fills.size(), 0u);
    EXPECT_EQ(exchange.pending_order_count(), 1u);

    // Market moves: ask drops to 100.00 → should fill
    exchange.on_market_update(sym, 99'5000, 100'0000);

    EXPECT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].order_id, 30u);
    EXPECT_EQ(fills[0].fill_price, 100'0000u);  // at limit price
    EXPECT_EQ(fills[0].fill_quantity, 25u);
    EXPECT_EQ(exchange.pending_order_count(), 0u);
}
