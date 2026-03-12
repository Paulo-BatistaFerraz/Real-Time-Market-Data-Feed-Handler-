#include <gtest/gtest.h>
#include "qf/risk/limits/loss_limit.hpp"
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

strategy::Decision make_decision(const char* sym, Side side, Quantity qty) {
    return strategy::Decision{
        make_symbol(sym), side, qty,
        strategy::OrderType::Market, 0, strategy::Urgency::Medium};
}

}  // namespace

// --- Basic loss limit checks ---

TEST(LossLimitTest, PassesWhenNoPnL) {
    LossLimit ll(10000.0);
    strategy::Portfolio portfolio;

    auto result = ll.check(make_decision("AAPL", Side::Buy, 100), portfolio);
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.check_type, RiskCheck::LossLimit);
}

TEST(LossLimitTest, PassesWhenPnLPositive) {
    LossLimit ll(10000.0);
    strategy::Portfolio portfolio;

    // Simulate a profitable fill.
    Symbol aapl = make_symbol("AAPL");
    portfolio.on_fill(0, aapl, Side::Buy, double_to_price(100.0), 100);
    portfolio.mark_to_market(aapl, double_to_price(110.0));

    auto result = ll.check(make_decision("AAPL", Side::Buy, 100), portfolio);
    EXPECT_TRUE(result.passed);
}

TEST(LossLimitTest, BlocksOrderWhenDailyLossExceeded) {
    LossLimit ll(1000.0);
    strategy::Portfolio portfolio;

    // Record a large running loss.
    ll.set_running_pnl(-500.0);

    // Portfolio also has a loss that pushes total beyond limit.
    Symbol aapl = make_symbol("AAPL");
    portfolio.on_fill(0, aapl, Side::Buy, double_to_price(100.0), 100);
    portfolio.mark_to_market(aapl, double_to_price(94.0));
    // Unrealized PnL = (94 - 100) * 100 = -$600
    // Running PnL = -500 + portfolio(-600) = -1100 < -1000

    auto result = ll.check(make_decision("AAPL", Side::Buy, 100), portfolio);
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.check_type, RiskCheck::LossLimit);
}

TEST(LossLimitTest, BlocksWhenRunningPnlAloneExceedsLimit) {
    LossLimit ll(5000.0);
    strategy::Portfolio portfolio;  // empty portfolio, 0 pnl

    // Running PnL alone exceeds the limit.
    ll.set_running_pnl(-5001.0);

    auto result = ll.check(make_decision("AAPL", Side::Buy, 100), portfolio);
    EXPECT_FALSE(result.passed);
}

TEST(LossLimitTest, PassesAtExactBoundary) {
    LossLimit ll(5000.0);
    strategy::Portfolio portfolio;

    // Effective PnL of exactly -5000.0 is NOT > -5000.0, so it should fail.
    ll.set_running_pnl(-5000.0);

    auto result = ll.check(make_decision("AAPL", Side::Buy, 100), portfolio);
    EXPECT_FALSE(result.passed);
}

TEST(LossLimitTest, PassesJustAboveBoundary) {
    LossLimit ll(5000.0);
    strategy::Portfolio portfolio;

    // Effective PnL of -4999.0 > -5000.0, so passes.
    ll.set_running_pnl(-4999.0);

    auto result = ll.check(make_decision("AAPL", Side::Buy, 100), portfolio);
    EXPECT_TRUE(result.passed);
}

// --- Record PnL ---

TEST(LossLimitTest, RecordPnlAccumulates) {
    LossLimit ll(10000.0);
    ll.record_pnl(-3000.0);
    ll.record_pnl(-2000.0);
    EXPECT_DOUBLE_EQ(ll.running_pnl(), -5000.0);
}

// --- Reset ---

TEST(LossLimitTest, ResetClearsPnL) {
    LossLimit ll(10000.0);
    ll.set_running_pnl(-8000.0);
    ll.reset();
    EXPECT_DOUBLE_EQ(ll.running_pnl(), 0.0);
}

// --- Auto-reset interval ---

TEST(LossLimitTest, AutoResetOnIntervalBoundary) {
    // Reset interval of 1 second (1,000,000,000 ns).
    LossLimit ll(5000.0, 1'000'000'000ULL);
    strategy::Portfolio portfolio;

    ll.set_running_pnl(-3000.0);

    // First check at t=100ns initializes the reset timer.
    auto r1 = ll.check(make_decision("AAPL", Side::Buy, 100), portfolio, 100);
    // PnL = -3000 + 0 = -3000 > -5000, so passes.
    EXPECT_TRUE(r1.passed);

    // Second check at t=1'000'000'200 should trigger auto-reset (crossed 1s boundary).
    auto r2 = ll.check(make_decision("AAPL", Side::Buy, 100), portfolio, 1'000'000'200ULL);
    // After reset, running_pnl should be 0, portfolio pnl is 0 => passes.
    EXPECT_TRUE(r2.passed);
    EXPECT_DOUBLE_EQ(ll.running_pnl(), 0.0);
}
