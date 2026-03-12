#include <gtest/gtest.h>
#include <cmath>
#include "qf/risk/monitors/var_calculator.hpp"
#include "qf/strategy/position/portfolio.hpp"

using namespace qf;
using namespace qf::risk;

namespace {

Symbol make_symbol(const char* name) {
    Symbol s{};
    for (int i = 0; i < 8 && name[i]; ++i) s.data[i] = name[i];
    return s;
}

}  // namespace

// --- Hand-computed VaR verification ---
//
// Formula: VaR = |position_value| * volatility * z_score * sqrt(holding_period)
//
// Parameters:
//   position  = 1000 shares of AAPL at $150.00
//   position_value = 1000 * 150.00 = $150,000
//   volatility = 0.02 (2% daily vol, set directly)
//   confidence = 95% => z_score = 1.645
//   holding_period = 1 day => sqrt(1) = 1.0
//
// Expected VaR = 150000 * 0.02 * 1.645 * 1.0 = $4935.00

TEST(VaRCalculatorTest, HandComputedSingleSymbol) {
    VaRConfig config;
    config.confidence_level = 0.95;
    config.holding_period_days = 1;

    VaRCalculator calc(config);

    uint64_t aapl_key = make_symbol("AAPL").as_key();
    calc.set_price(aapl_key, double_to_price(150.0));
    calc.set_volatility(aapl_key, 0.02);

    std::unordered_map<uint64_t, int64_t> positions;
    positions[aapl_key] = 1000;

    auto result = calc.compute(positions);

    ASSERT_EQ(result.per_symbol.size(), 1u);
    EXPECT_DOUBLE_EQ(result.per_symbol[0].position_value, 150000.0);
    EXPECT_DOUBLE_EQ(result.per_symbol[0].volatility, 0.02);

    // Expected VaR = 150000 * 0.02 * 1.645 * 1.0 = 4935.0
    double expected_var = 150000.0 * 0.02 * 1.645 * 1.0;
    EXPECT_NEAR(result.per_symbol[0].var_dollars, expected_var, 0.01);
    EXPECT_NEAR(result.portfolio_var, expected_var, 0.01);
    EXPECT_NEAR(result.undiversified_var, expected_var, 0.01);
    EXPECT_NEAR(result.diversification_benefit, 0.0, 0.01);
}

// --- Two-symbol hand computation ---
//
// AAPL: 1000 shares @ $150, vol=0.02 => VaR_A = 150000*0.02*1.645 = $4935.00
// MSFT: 500 shares @ $300, vol=0.03  => VaR_M = 150000*0.03*1.645 = $7402.50
//
// Without correlation: VaR_p = sqrt(4935^2 + 7402.5^2) = sqrt(24354225 + 54797006.25)
//                             = sqrt(79151231.25) ≈ $8896.70

TEST(VaRCalculatorTest, TwoSymbolIndependenceAssumption) {
    VaRConfig config;
    config.confidence_level = 0.95;
    config.holding_period_days = 1;

    VaRCalculator calc(config);

    uint64_t aapl_key = make_symbol("AAPL").as_key();
    uint64_t msft_key = make_symbol("MSFT").as_key();

    calc.set_price(aapl_key, double_to_price(150.0));
    calc.set_price(msft_key, double_to_price(300.0));
    calc.set_volatility(aapl_key, 0.02);
    calc.set_volatility(msft_key, 0.03);

    std::unordered_map<uint64_t, int64_t> positions;
    positions[aapl_key] = 1000;
    positions[msft_key] = 500;

    auto result = calc.compute(positions);

    ASSERT_EQ(result.per_symbol.size(), 2u);
    EXPECT_FALSE(result.uses_correlation);

    double var_a = 150000.0 * 0.02 * 1.645;
    double var_m = 150000.0 * 0.03 * 1.645;
    double expected_portfolio = std::sqrt(var_a * var_a + var_m * var_m);
    double expected_undiverse = var_a + var_m;

    EXPECT_NEAR(result.undiversified_var, expected_undiverse, 0.01);
    EXPECT_NEAR(result.portfolio_var, expected_portfolio, 0.01);
    EXPECT_GT(result.diversification_benefit, 0.0);
}

// --- With correlation matrix ---
//
// Same positions, but with correlation = 0.5 between AAPL and MSFT.
// VaR_p = sqrt(VaR_A^2 + VaR_M^2 + 2*VaR_A*VaR_M*0.5)
//       = sqrt(4935^2 + 7402.5^2 + 2*4935*7402.5*0.5)

TEST(VaRCalculatorTest, TwoSymbolWithCorrelation) {
    VaRConfig config;
    config.confidence_level = 0.95;
    config.holding_period_days = 1;

    VaRCalculator calc(config);

    uint64_t aapl_key = make_symbol("AAPL").as_key();
    uint64_t msft_key = make_symbol("MSFT").as_key();

    calc.set_price(aapl_key, double_to_price(150.0));
    calc.set_price(msft_key, double_to_price(300.0));
    calc.set_volatility(aapl_key, 0.02);
    calc.set_volatility(msft_key, 0.03);
    calc.set_correlation(aapl_key, msft_key, 0.5);

    std::unordered_map<uint64_t, int64_t> positions;
    positions[aapl_key] = 1000;
    positions[msft_key] = 500;

    auto result = calc.compute(positions);

    EXPECT_TRUE(result.uses_correlation);

    double var_a = 150000.0 * 0.02 * 1.645;
    double var_m = 150000.0 * 0.03 * 1.645;
    double expected = std::sqrt(var_a * var_a + var_m * var_m + 2.0 * var_a * var_m * 0.5);

    EXPECT_NEAR(result.portfolio_var, expected, 0.01);
}

// --- Holding period scaling ---
//
// VaR scales by sqrt(holding_period).
// 5-day VaR should be sqrt(5) times 1-day VaR.

TEST(VaRCalculatorTest, HoldingPeriodScaling) {
    VaRConfig config1;
    config1.confidence_level = 0.95;
    config1.holding_period_days = 1;

    VaRConfig config5;
    config5.confidence_level = 0.95;
    config5.holding_period_days = 5;

    VaRCalculator calc1(config1);
    VaRCalculator calc5(config5);

    uint64_t key = make_symbol("AAPL").as_key();
    calc1.set_price(key, double_to_price(100.0));
    calc1.set_volatility(key, 0.01);
    calc5.set_price(key, double_to_price(100.0));
    calc5.set_volatility(key, 0.01);

    std::unordered_map<uint64_t, int64_t> pos;
    pos[key] = 1000;

    auto r1 = calc1.compute(pos);
    auto r5 = calc5.compute(pos);

    EXPECT_NEAR(r5.portfolio_var, r1.portfolio_var * std::sqrt(5.0), 0.01);
}

// --- Confidence level z-scores ---

TEST(VaRCalculatorTest, HigherConfidenceHigherVaR) {
    VaRConfig config95;
    config95.confidence_level = 0.95;
    VaRConfig config99;
    config99.confidence_level = 0.99;

    VaRCalculator calc95(config95);
    VaRCalculator calc99(config99);

    uint64_t key = make_symbol("AAPL").as_key();
    calc95.set_price(key, double_to_price(100.0));
    calc95.set_volatility(key, 0.02);
    calc99.set_price(key, double_to_price(100.0));
    calc99.set_volatility(key, 0.02);

    std::unordered_map<uint64_t, int64_t> pos;
    pos[key] = 1000;

    auto r95 = calc95.compute(pos);
    auto r99 = calc99.compute(pos);

    EXPECT_GT(r99.portfolio_var, r95.portfolio_var);
}

// --- Volatility from return history ---

TEST(VaRCalculatorTest, VolatilityComputedFromReturns) {
    VaRConfig config;
    config.confidence_level = 0.95;
    config.holding_period_days = 1;
    config.lookback_periods = 5;

    VaRCalculator calc(config);

    uint64_t key = make_symbol("AAPL").as_key();
    calc.set_price(key, double_to_price(100.0));

    // Add known returns: 0.01, -0.01, 0.02, -0.02, 0.01
    // Mean = 0.002, sum_sq_diff = (0.008^2 + 0.012^2 + 0.018^2 + 0.022^2 + 0.008^2)
    // = 0.000064 + 0.000144 + 0.000324 + 0.000484 + 0.000064 = 0.001080
    // Sample std dev = sqrt(0.001080 / 4) = sqrt(0.000270) ≈ 0.016432
    calc.add_return(key, 0.01);
    calc.add_return(key, -0.01);
    calc.add_return(key, 0.02);
    calc.add_return(key, -0.02);
    calc.add_return(key, 0.01);

    std::unordered_map<uint64_t, int64_t> pos;
    pos[key] = 1000;

    auto result = calc.compute(pos);
    ASSERT_EQ(result.per_symbol.size(), 1u);

    // VaR = |1000 * 100| * vol * 1.645 * 1
    double expected_vol = std::sqrt(0.00108 / 4.0);
    double expected_var = 100000.0 * expected_vol * 1.645;
    EXPECT_NEAR(result.portfolio_var, expected_var, 1.0);
}

// --- Zero position ---

TEST(VaRCalculatorTest, ZeroPositionSkipped) {
    VaRConfig config;
    VaRCalculator calc(config);

    uint64_t key = make_symbol("AAPL").as_key();
    calc.set_price(key, double_to_price(100.0));
    calc.set_volatility(key, 0.02);

    std::unordered_map<uint64_t, int64_t> pos;
    pos[key] = 0;

    auto result = calc.compute(pos);
    EXPECT_EQ(result.per_symbol.size(), 0u);
    EXPECT_DOUBLE_EQ(result.portfolio_var, 0.0);
}

// --- Short position has same VaR magnitude ---

TEST(VaRCalculatorTest, ShortPositionSameVaR) {
    VaRConfig config;
    config.confidence_level = 0.95;
    config.holding_period_days = 1;

    VaRCalculator calc_long(config);
    VaRCalculator calc_short(config);

    uint64_t key = make_symbol("AAPL").as_key();
    calc_long.set_price(key, double_to_price(100.0));
    calc_long.set_volatility(key, 0.02);
    calc_short.set_price(key, double_to_price(100.0));
    calc_short.set_volatility(key, 0.02);

    std::unordered_map<uint64_t, int64_t> pos_long, pos_short;
    pos_long[key] = 1000;
    pos_short[key] = -1000;

    auto r_long = calc_long.compute(pos_long);
    auto r_short = calc_short.compute(pos_short);

    EXPECT_NEAR(r_long.portfolio_var, r_short.portfolio_var, 0.01);
}
