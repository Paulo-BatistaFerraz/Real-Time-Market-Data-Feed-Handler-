#include <gtest/gtest.h>
#include "qf/strategy/execution/order_sizer.hpp"
#include "qf/common/types.hpp"
#include <cmath>

using namespace qf;
using namespace qf::strategy;

class OrderSizerTest : public ::testing::Test {
protected:
    PortfolioView make_portfolio(double cash = 100000.0) {
        PortfolioView p;
        p.cash = cash;
        return p;
    }

    RiskBudget default_budget() {
        RiskBudget rb;
        rb.win_rate = 0.55;
        rb.win_loss_ratio = 1.5;
        rb.max_portfolio_risk = 0.1;
        return rb;
    }
};

// --- Fixed sizing tests ---

TEST_F(OrderSizerTest, FixedSizingFullStrength) {
    OrderSizer sizer(SizingMethod::Fixed);
    sizer.set_fixed_size(100);

    Quantity qty = sizer.size(1.0, make_portfolio(), default_budget());
    EXPECT_EQ(qty, 100u);
}

TEST_F(OrderSizerTest, FixedSizingHalfStrength) {
    OrderSizer sizer(SizingMethod::Fixed);
    sizer.set_fixed_size(100);

    Quantity qty = sizer.size(0.5, make_portfolio(), default_budget());
    EXPECT_EQ(qty, 50u);
}

TEST_F(OrderSizerTest, FixedSizingNegativeStrength) {
    OrderSizer sizer(SizingMethod::Fixed);
    sizer.set_fixed_size(100);

    // Negative strength → uses abs value for sizing
    Quantity qty = sizer.size(-0.5, make_portfolio(), default_budget());
    EXPECT_EQ(qty, 50u);
}

TEST_F(OrderSizerTest, FixedSizingZeroStrengthReturnsZero) {
    OrderSizer sizer(SizingMethod::Fixed);
    sizer.set_fixed_size(100);

    Quantity qty = sizer.size(0.0, make_portfolio(), default_budget());
    EXPECT_EQ(qty, 0u);
}

// --- Max position limit tests ---

TEST_F(OrderSizerTest, RespectsMaxPosition) {
    OrderSizer sizer(SizingMethod::Fixed);
    sizer.set_fixed_size(1000);
    sizer.set_max_position(500);

    Quantity qty = sizer.size(1.0, make_portfolio(), default_budget());
    EXPECT_EQ(qty, 500u);
}

TEST_F(OrderSizerTest, MaxPositionClampsOutput) {
    OrderSizer sizer(SizingMethod::Fixed);
    sizer.set_fixed_size(200);
    sizer.set_max_position(50);

    Quantity qty = sizer.size(1.0, make_portfolio(), default_budget());
    EXPECT_EQ(qty, 50u);
}

TEST_F(OrderSizerTest, MaxPositionAllowsSmallerOrders) {
    OrderSizer sizer(SizingMethod::Fixed);
    sizer.set_fixed_size(30);
    sizer.set_max_position(100);

    Quantity qty = sizer.size(1.0, make_portfolio(), default_budget());
    EXPECT_EQ(qty, 30u);
}

// --- Fixed-fractional sizing tests ---

TEST_F(OrderSizerTest, FractionalSizing) {
    OrderSizer sizer(SizingMethod::FixedFractional);
    sizer.set_fractional_pct(0.02);  // 2% of cash

    // cash=100000, 2% = 2000, full strength → 2000
    Quantity qty = sizer.size(1.0, make_portfolio(100000.0), default_budget());
    EXPECT_EQ(qty, 2000u);
}

TEST_F(OrderSizerTest, FractionalSizingScaledByStrength) {
    OrderSizer sizer(SizingMethod::FixedFractional);
    sizer.set_fractional_pct(0.02);

    // cash=100000, 2% * 0.5 = 1000
    Quantity qty = sizer.size(0.5, make_portfolio(100000.0), default_budget());
    EXPECT_EQ(qty, 1000u);
}

TEST_F(OrderSizerTest, FractionalSizingZeroCash) {
    OrderSizer sizer(SizingMethod::FixedFractional);
    sizer.set_fractional_pct(0.02);

    Quantity qty = sizer.size(1.0, make_portfolio(0.0), default_budget());
    EXPECT_EQ(qty, 0u);
}

TEST_F(OrderSizerTest, FractionalRespectsMaxPosition) {
    OrderSizer sizer(SizingMethod::FixedFractional);
    sizer.set_fractional_pct(0.1);  // 10%
    sizer.set_max_position(500);

    // cash=100000, 10% = 10000, clamped to 500
    Quantity qty = sizer.size(1.0, make_portfolio(100000.0), default_budget());
    EXPECT_EQ(qty, 500u);
}

// --- Kelly sizing tests ---

TEST_F(OrderSizerTest, KellySizing) {
    OrderSizer sizer(SizingMethod::Kelly);
    sizer.set_max_position(100000);  // raise limit so Kelly result isn't clamped

    RiskBudget rb;
    rb.win_rate = 0.6;
    rb.win_loss_ratio = 2.0;
    rb.max_portfolio_risk = 0.5;

    // Kelly f* = p - q/b = 0.6 - 0.4/2.0 = 0.6 - 0.2 = 0.4
    // capped by max_portfolio_risk = 0.5 → 0.4
    // notional = 100000 * 0.4 * 1.0 = 40000
    Quantity qty = sizer.size(1.0, make_portfolio(100000.0), rb);
    EXPECT_EQ(qty, 40000u);
}

TEST_F(OrderSizerTest, KellyCapByMaxPortfolioRisk) {
    OrderSizer sizer(SizingMethod::Kelly);

    RiskBudget rb;
    rb.win_rate = 0.9;
    rb.win_loss_ratio = 10.0;
    rb.max_portfolio_risk = 0.05;  // very conservative cap

    // Kelly f* = 0.9 - 0.1/10 = 0.89, capped to 0.05
    // notional = 100000 * 0.05 * 1.0 = 5000
    Quantity qty = sizer.size(1.0, make_portfolio(100000.0), rb);
    EXPECT_EQ(qty, 5000u);
}

TEST_F(OrderSizerTest, KellyNegativeEdgeReturnsZero) {
    OrderSizer sizer(SizingMethod::Kelly);

    RiskBudget rb;
    rb.win_rate = 0.3;        // poor edge
    rb.win_loss_ratio = 0.5;  // win_loss ratio < 1
    rb.max_portfolio_risk = 0.1;

    // Kelly f* = 0.3 - 0.7/0.5 = 0.3 - 1.4 = -1.1 → no bet
    Quantity qty = sizer.size(1.0, make_portfolio(100000.0), rb);
    EXPECT_EQ(qty, 0u);
}

TEST_F(OrderSizerTest, KellyRespectsMaxPosition) {
    OrderSizer sizer(SizingMethod::Kelly);
    sizer.set_max_position(100);

    RiskBudget rb;
    rb.win_rate = 0.6;
    rb.win_loss_ratio = 2.0;
    rb.max_portfolio_risk = 0.5;

    // Kelly would yield 40000, but clamped to 100
    Quantity qty = sizer.size(1.0, make_portfolio(100000.0), rb);
    EXPECT_EQ(qty, 100u);
}

// --- Configuration tests ---

TEST_F(OrderSizerTest, ConfigureFromYaml) {
    OrderSizer sizer;

    auto cfg = Config::parse(
        "order_sizer:\n"
        "  method: kelly\n"
        "  fixed_size: 50\n"
        "  fractional_pct: 0.05\n"
        "  max_position: 250\n");
    sizer.configure(cfg);

    EXPECT_EQ(sizer.method(), SizingMethod::Kelly);
    EXPECT_EQ(sizer.max_position(), 250u);
}

TEST_F(OrderSizerTest, AccessorMaxPosition) {
    OrderSizer sizer;
    sizer.set_max_position(777);
    EXPECT_EQ(sizer.max_position(), 777u);
}
