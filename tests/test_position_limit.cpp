#include <gtest/gtest.h>
#include "qf/risk/limits/position_limit.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

using namespace qf;
using namespace qf::risk;

namespace {

Symbol make_symbol(const char* name) {
    Symbol s{};
    for (int i = 0; i < 8 && name[i]; ++i) s.data[i] = name[i];
    return s;
}

strategy::Decision make_decision(const char* sym, Side side, Quantity qty,
                                 strategy::OrderType ot = strategy::OrderType::Market,
                                 Price limit_price = 0) {
    return strategy::Decision{
        make_symbol(sym), side, qty, ot, limit_price, strategy::Urgency::Medium};
}

}  // namespace

// --- Basic limit checks ---

TEST(PositionLimitTest, AllowsOrderWithinLimit) {
    PositionLimit pl(1000);
    strategy::Portfolio portfolio;

    auto result = pl.check(make_decision("AAPL", Side::Buy, 500), portfolio);
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.check_type, RiskCheck::PositionLimit);
}

TEST(PositionLimitTest, AllowsOrderExactlyAtLimit) {
    PositionLimit pl(1000);
    strategy::Portfolio portfolio;

    auto result = pl.check(make_decision("AAPL", Side::Buy, 1000), portfolio);
    EXPECT_TRUE(result.passed);
}

TEST(PositionLimitTest, BlocksOrderExceedingMax) {
    PositionLimit pl(1000);
    strategy::Portfolio portfolio;

    auto result = pl.check(make_decision("AAPL", Side::Buy, 1001), portfolio);
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.check_type, RiskCheck::PositionLimit);
    EXPECT_GT(result.current_value, result.limit_value);
}

TEST(PositionLimitTest, BlocksWhenExistingPositionPlusDeltaExceeds) {
    PositionLimit pl(1000);
    strategy::Portfolio portfolio;

    // Build up a position of 800 shares.
    Symbol aapl = make_symbol("AAPL");
    portfolio.on_fill(0, aapl, Side::Buy, double_to_price(150.0), 800);

    // Trying to buy 201 more should fail (800 + 201 = 1001 > 1000).
    auto result = pl.check(make_decision("AAPL", Side::Buy, 201), portfolio);
    EXPECT_FALSE(result.passed);
    EXPECT_DOUBLE_EQ(result.current_value, 1001.0);
    EXPECT_DOUBLE_EQ(result.limit_value, 1000.0);
}

TEST(PositionLimitTest, AllowsSellWhenLong) {
    PositionLimit pl(1000);
    strategy::Portfolio portfolio;

    Symbol aapl = make_symbol("AAPL");
    portfolio.on_fill(0, aapl, Side::Buy, double_to_price(150.0), 800);

    // Selling 500 reduces position to 300, well within limit.
    auto result = pl.check(make_decision("AAPL", Side::Sell, 500), portfolio);
    EXPECT_TRUE(result.passed);
}

TEST(PositionLimitTest, BlocksShortPositionExceedingLimit) {
    PositionLimit pl(1000);
    strategy::Portfolio portfolio;

    // Selling 1001 short exceeds limit.
    auto result = pl.check(make_decision("AAPL", Side::Sell, 1001), portfolio);
    EXPECT_FALSE(result.passed);
}

// --- Per-symbol overrides ---

TEST(PositionLimitTest, PerSymbolOverrideLowerLimit) {
    PositionLimit pl(1000);
    pl.set_symbol_limit(make_symbol("TSLA"), 500);
    strategy::Portfolio portfolio;

    // TSLA has a 500 limit — 501 should fail.
    auto result = pl.check(make_decision("TSLA", Side::Buy, 501), portfolio);
    EXPECT_FALSE(result.passed);

    // But AAPL still uses global 1000 — 501 should pass.
    auto result2 = pl.check(make_decision("AAPL", Side::Buy, 501), portfolio);
    EXPECT_TRUE(result2.passed);
}

// --- Accessor ---

TEST(PositionLimitTest, GlobalMaxAccessor) {
    PositionLimit pl(5000);
    EXPECT_EQ(pl.global_max(), 5000);
}
