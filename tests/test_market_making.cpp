#include <gtest/gtest.h>
#include "qf/strategy/strategies/market_making.hpp"
#include "qf/strategy/strategy_engine.hpp"
#include "qf/common/types.hpp"
#include <memory>

using namespace qf;
using namespace qf::strategy;
using namespace qf::signals;

class MarketMakingTest : public ::testing::Test {
protected:
    MarketMakingStrategy mm;
    Symbol sym{"AAPL"};

    Signal make_signal(uint64_t fair_value_price, double strength = 0.5) {
        Signal sig;
        sig.symbol = sym;
        sig.type = SignalType::Composite;
        sig.strength = strength;
        sig.direction = SignalDirection::Buy;
        sig.timestamp = fair_value_price;
        return sig;
    }

    PortfolioView make_portfolio(int64_t position = 0) {
        PortfolioView p;
        std::string s(sym.data, strnlen(sym.data, SYMBOL_LENGTH));
        p.positions[s] = position;
        p.cash = 1000000.0;
        return p;
    }
};

// Test: quotes both sides when flat
TEST_F(MarketMakingTest, QuotesBothSidesWhenFlat) {
    auto decisions = mm.on_signal(make_signal(1000000), make_portfolio(0));
    ASSERT_EQ(decisions.size(), 2u);

    // One buy, one sell
    bool has_buy = false, has_sell = false;
    for (const auto& d : decisions) {
        if (d.side == Side::Buy) has_buy = true;
        if (d.side == Side::Sell) has_sell = true;
        EXPECT_EQ(d.order_type, OrderType::Limit);
        EXPECT_EQ(d.quantity, 100u);
    }
    EXPECT_TRUE(has_buy);
    EXPECT_TRUE(has_sell);
}

// Test: bid is below ask (spread maintained)
TEST_F(MarketMakingTest, BidBelowAsk) {
    auto decisions = mm.on_signal(make_signal(1000000), make_portfolio(0));
    ASSERT_EQ(decisions.size(), 2u);

    Price bid = 0, ask = 0;
    for (const auto& d : decisions) {
        if (d.side == Side::Buy) bid = d.limit_price;
        if (d.side == Side::Sell) ask = d.limit_price;
    }
    EXPECT_LT(bid, ask);
}

// Test: inventory skew shifts quotes when long
TEST_F(MarketMakingTest, InventorySkewWhenLong) {
    auto flat_decisions = mm.on_signal(make_signal(1000000), make_portfolio(0));
    auto long_decisions = mm.on_signal(make_signal(1000000), make_portfolio(100));

    Price flat_bid = 0, flat_ask = 0;
    Price long_bid = 0, long_ask = 0;
    for (const auto& d : flat_decisions) {
        if (d.side == Side::Buy) flat_bid = d.limit_price;
        if (d.side == Side::Sell) flat_ask = d.limit_price;
    }
    for (const auto& d : long_decisions) {
        if (d.side == Side::Buy) long_bid = d.limit_price;
        if (d.side == Side::Sell) long_ask = d.limit_price;
    }

    // When long, skew is negative → prices shift down
    EXPECT_LT(long_bid, flat_bid);
    EXPECT_LT(long_ask, flat_ask);
}

// Test: stops quoting buy side at max position
TEST_F(MarketMakingTest, StopsQuotingBuyAtMaxPosition) {
    // Default max_position is 1000, quote_size is 100
    // At position 950, buying 100 would reach 1050 > 1000 → no bid
    auto decisions = mm.on_signal(make_signal(1000000), make_portfolio(950));

    for (const auto& d : decisions) {
        EXPECT_NE(d.side, Side::Buy) << "Should not quote bid at near-max long position";
    }
    // Should still have at least the sell side
    EXPECT_GE(decisions.size(), 1u);
}

// Test: stops quoting sell side at max short position
TEST_F(MarketMakingTest, StopsQuotingSellAtMaxShortPosition) {
    auto decisions = mm.on_signal(make_signal(1000000), make_portfolio(-950));

    for (const auto& d : decisions) {
        EXPECT_NE(d.side, Side::Sell) << "Should not quote ask at near-max short position";
    }
    EXPECT_GE(decisions.size(), 1u);
}

// Test: no quotes when fair value is zero
TEST_F(MarketMakingTest, NoQuotesWhenFairValueZero) {
    auto decisions = mm.on_signal(make_signal(0), make_portfolio(0));
    EXPECT_TRUE(decisions.empty());
}

// Test: configure updates parameters
TEST_F(MarketMakingTest, ConfigureUpdatesParams) {
    auto cfg = Config::parse(
        "strategy:\n"
        "  market_making:\n"
        "    spread_ticks: 5\n"
        "    quote_size: 200\n"
        "    max_position: 500\n"
        "    inventory_skew_factor: 1.0\n");
    mm.configure(cfg);

    EXPECT_EQ(mm.spread_ticks(), 5u);
    EXPECT_EQ(mm.quote_size(), 200u);
    EXPECT_EQ(mm.max_position(), 500);
    EXPECT_DOUBLE_EQ(mm.inventory_skew_factor(), 1.0);
}

// Test: name returns correct identifier
TEST_F(MarketMakingTest, NameReturnsCorrectId) {
    EXPECT_EQ(mm.name(), "market_making");
}

// Test: strategy engine integration — on_signal fans to active strategies
TEST_F(MarketMakingTest, EngineOnSignalFansToActiveStrategies) {
    StrategyEngine engine;
    engine.registry().register_strategy("mm",
        std::make_unique<MarketMakingStrategy>());

    auto decisions = engine.on_signal(make_signal(1000000), make_portfolio(0));
    // Market making produces 2 decisions (bid+ask) when flat
    EXPECT_EQ(decisions.size(), 2u);
    EXPECT_EQ(engine.last_decisions().size(), 2u);
}

// Test: strategy engine enable/disable
TEST_F(MarketMakingTest, EngineEnableDisable) {
    StrategyEngine engine;
    engine.registry().register_strategy("mm",
        std::make_unique<MarketMakingStrategy>());

    // Disable → no decisions
    engine.disable("mm");
    auto decisions = engine.on_signal(make_signal(1000000), make_portfolio(0));
    EXPECT_TRUE(decisions.empty());

    // Re-enable → decisions again
    engine.enable("mm");
    decisions = engine.on_signal(make_signal(1000000), make_portfolio(0));
    EXPECT_EQ(decisions.size(), 2u);
}

// Test: strategy engine configure
TEST_F(MarketMakingTest, EngineConfigureUpdatesStrategy) {
    StrategyEngine engine;
    engine.registry().register_strategy("mm",
        std::make_unique<MarketMakingStrategy>());

    auto cfg = Config::parse(
        "strategy:\n"
        "  market_making:\n"
        "    quote_size: 50\n");
    engine.configure("mm", cfg);

    auto decisions = engine.on_signal(make_signal(1000000), make_portfolio(0));
    for (const auto& d : decisions) {
        EXPECT_EQ(d.quantity, 50u);
    }
}

// Test: OMS callback receives decisions
TEST_F(MarketMakingTest, OmsCallbackReceivesDecisions) {
    StrategyEngine engine;
    engine.registry().register_strategy("mm",
        std::make_unique<MarketMakingStrategy>());

    std::vector<Decision> captured;
    engine.set_oms_callback([&](const std::vector<Decision>& ds) {
        captured = ds;
    });

    engine.on_signal(make_signal(1000000), make_portfolio(0));
    EXPECT_EQ(captured.size(), 2u);
}
