#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// VaR configuration parameters.
struct VaRConfig {
    double confidence_level{0.95};     // 0.95 or 0.99
    uint32_t holding_period_days{1};   // Holding period in days
    uint32_t lookback_periods{20};     // Number of periods for volatility estimation
};

// Per-symbol VaR result.
struct SymbolVaR {
    uint64_t symbol_key{0};
    double   position_value{0.0};   // Dollar position (signed)
    double   volatility{0.0};       // Annualized or period volatility used
    double   var_dollars{0.0};      // VaR in dollar terms (positive = loss)
};

// Portfolio VaR result.
struct PortfolioVaRResult {
    std::vector<SymbolVaR> per_symbol;
    double portfolio_var{0.0};           // Total portfolio VaR in dollars
    double undiversified_var{0.0};       // Sum of individual VaRs (no correlation benefit)
    double diversification_benefit{0.0}; // undiversified - portfolio
    bool   uses_correlation{false};      // True if correlation matrix was provided
};

// VaRCalculator — parametric Value-at-Risk.
//
// Assumes normally distributed returns.
// VaR = |position_value| * volatility * z_score * sqrt(holding_period)
//
// Computes per-symbol and portfolio VaR. Portfolio VaR accounts for
// correlations when a correlation matrix is provided; otherwise assumes
// independence (sum of squared individual VaRs).
class VaRCalculator {
public:
    explicit VaRCalculator(const VaRConfig& config = VaRConfig{});

    // Set the confidence level (0.95 or 0.99).
    void set_confidence(double level);

    // Set holding period in days.
    void set_holding_period(uint32_t days);

    // Set lookback for volatility estimation.
    void set_lookback(uint32_t periods);

    // Record a return observation for a symbol.
    // Returns are simple percentage returns (e.g. 0.01 = 1%).
    void add_return(uint64_t symbol_key, double ret);

    // Set per-symbol volatility directly (overrides computed volatility).
    void set_volatility(uint64_t symbol_key, double vol);

    // Set the correlation matrix between symbols.
    // Keys are pairs (sym_a, sym_b) where sym_a <= sym_b.
    void set_correlation(uint64_t sym_a, uint64_t sym_b, double corr);

    // Set reference price for a symbol (fixed-point Price).
    void set_price(uint64_t symbol_key, Price price);

    // Compute VaR given current portfolio positions.
    PortfolioVaRResult compute(const strategy::Portfolio& portfolio) const;

    // Compute VaR from explicit position map (symbol_key -> signed quantity).
    PortfolioVaRResult compute(const std::unordered_map<uint64_t, int64_t>& positions) const;

    // Accessors
    double confidence_level() const { return config_.confidence_level; }
    uint32_t holding_period() const { return config_.holding_period_days; }
    uint32_t lookback() const { return config_.lookback_periods; }

private:
    VaRConfig config_;

    // Z-score for the configured confidence level.
    double z_score() const;

    // Compute volatility for a symbol from recorded returns.
    double compute_volatility(uint64_t symbol_key) const;

    // Get the correlation between two symbols.
    // Returns 0.0 if not set (independence assumption).
    double get_correlation(uint64_t sym_a, uint64_t sym_b) const;

    // Per-symbol return history for volatility estimation.
    std::unordered_map<uint64_t, std::vector<double>> returns_;

    // Overridden volatilities (takes precedence over computed).
    std::unordered_map<uint64_t, double> volatilities_;

    // Correlation matrix: key = (min(a,b), max(a,b)) packed.
    // We use a map keyed by a combined 128-bit-like key packed into a struct.
    struct PairKey {
        uint64_t a, b;
        bool operator==(const PairKey& o) const { return a == o.a && b == o.b; }
    };
    struct PairKeyHash {
        size_t operator()(const PairKey& k) const {
            size_t h1 = std::hash<uint64_t>{}(k.a);
            size_t h2 = std::hash<uint64_t>{}(k.b);
            return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    std::unordered_map<PairKey, double, PairKeyHash> correlations_;

    // Reference prices: symbol_key -> Price (fixed-point).
    std::unordered_map<uint64_t, Price> prices_;
};

}  // namespace qf::risk
