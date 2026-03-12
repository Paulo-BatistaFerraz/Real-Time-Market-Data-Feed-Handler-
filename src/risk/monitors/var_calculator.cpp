#include "qf/risk/monitors/var_calculator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace qf::risk {

VaRCalculator::VaRCalculator(const VaRConfig& config)
    : config_(config) {}

void VaRCalculator::set_confidence(double level) {
    config_.confidence_level = level;
}

void VaRCalculator::set_holding_period(uint32_t days) {
    config_.holding_period_days = days;
}

void VaRCalculator::set_lookback(uint32_t periods) {
    config_.lookback_periods = periods;
}

void VaRCalculator::add_return(uint64_t symbol_key, double ret) {
    auto& hist = returns_[symbol_key];
    hist.push_back(ret);
    // Keep only the most recent lookback_periods.
    if (hist.size() > config_.lookback_periods) {
        hist.erase(hist.begin());
    }
}

void VaRCalculator::set_volatility(uint64_t symbol_key, double vol) {
    volatilities_[symbol_key] = vol;
}

void VaRCalculator::set_correlation(uint64_t sym_a, uint64_t sym_b, double corr) {
    uint64_t lo = std::min(sym_a, sym_b);
    uint64_t hi = std::max(sym_a, sym_b);
    correlations_[PairKey{lo, hi}] = corr;
}

void VaRCalculator::set_price(uint64_t symbol_key, Price price) {
    prices_[symbol_key] = price;
}

PortfolioVaRResult VaRCalculator::compute(const strategy::Portfolio& portfolio) const {
    auto snap = portfolio.snapshot();
    return compute(snap.positions);
}

PortfolioVaRResult VaRCalculator::compute(
    const std::unordered_map<uint64_t, int64_t>& positions) const {

    PortfolioVaRResult result;
    double z = z_score();
    double sqrt_hp = std::sqrt(static_cast<double>(config_.holding_period_days));

    // Compute per-symbol VaR.
    for (const auto& [sym_key, qty] : positions) {
        if (qty == 0) continue;

        // Get reference price in dollars.
        double price_dbl = 0.0;
        auto pit = prices_.find(sym_key);
        if (pit != prices_.end()) {
            price_dbl = price_to_double(pit->second);
        }

        double position_value = static_cast<double>(qty) * price_dbl;
        double vol = compute_volatility(sym_key);

        // Parametric VaR = |position_value| * volatility * z_score * sqrt(holding_period)
        double var = std::abs(position_value) * vol * z * sqrt_hp;

        result.per_symbol.push_back(SymbolVaR{
            sym_key,
            position_value,
            vol,
            var
        });
    }

    // Undiversified VaR = sum of individual VaRs.
    result.undiversified_var = 0.0;
    for (const auto& sv : result.per_symbol) {
        result.undiversified_var += sv.var_dollars;
    }

    // Portfolio VaR — check if we have correlation data.
    bool has_correlations = !correlations_.empty();
    result.uses_correlation = has_correlations;

    if (result.per_symbol.size() <= 1) {
        // Single asset: portfolio VaR = individual VaR.
        result.portfolio_var = result.undiversified_var;
    } else if (has_correlations) {
        // With correlation matrix: VaR_p = sqrt( sum_i sum_j VaR_i * VaR_j * corr(i,j) )
        double sum = 0.0;
        for (size_t i = 0; i < result.per_symbol.size(); ++i) {
            for (size_t j = 0; j < result.per_symbol.size(); ++j) {
                double corr;
                if (i == j) {
                    corr = 1.0;
                } else {
                    corr = get_correlation(
                        result.per_symbol[i].symbol_key,
                        result.per_symbol[j].symbol_key);
                }
                sum += result.per_symbol[i].var_dollars
                     * result.per_symbol[j].var_dollars
                     * corr;
            }
        }
        result.portfolio_var = std::sqrt(std::max(0.0, sum));
    } else {
        // Independence assumption: VaR_p = sqrt( sum of VaR_i^2 )
        double sum_sq = 0.0;
        for (const auto& sv : result.per_symbol) {
            sum_sq += sv.var_dollars * sv.var_dollars;
        }
        result.portfolio_var = std::sqrt(sum_sq);
    }

    result.diversification_benefit = result.undiversified_var - result.portfolio_var;

    return result;
}

double VaRCalculator::z_score() const {
    // Standard normal z-scores for common confidence levels.
    if (config_.confidence_level >= 0.99) return 2.326;
    if (config_.confidence_level >= 0.975) return 1.960;
    if (config_.confidence_level >= 0.95) return 1.645;
    if (config_.confidence_level >= 0.90) return 1.282;
    // Fallback: approximate inverse normal via Abramowitz & Stegun.
    // For 0 < p < 1: use rational approximation.
    double p = config_.confidence_level;
    double t = std::sqrt(-2.0 * std::log(1.0 - p));
    // Coefficients for rational approximation.
    constexpr double c0 = 2.515517;
    constexpr double c1 = 0.802853;
    constexpr double c2 = 0.010328;
    constexpr double d1 = 1.432788;
    constexpr double d2 = 0.189269;
    constexpr double d3 = 0.001308;
    return t - (c0 + c1 * t + c2 * t * t) / (1.0 + d1 * t + d2 * t * t + d3 * t * t * t);
}

double VaRCalculator::compute_volatility(uint64_t symbol_key) const {
    // Check for explicit override first.
    auto vit = volatilities_.find(symbol_key);
    if (vit != volatilities_.end()) {
        return vit->second;
    }

    // Compute from return history.
    auto rit = returns_.find(symbol_key);
    if (rit == returns_.end() || rit->second.size() < 2) {
        return 0.0;
    }

    const auto& rets = rit->second;
    size_t n = rets.size();

    // Mean.
    double mean = std::accumulate(rets.begin(), rets.end(), 0.0) / static_cast<double>(n);

    // Sample standard deviation.
    double sum_sq = 0.0;
    for (double r : rets) {
        double diff = r - mean;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / static_cast<double>(n - 1));
}

double VaRCalculator::get_correlation(uint64_t sym_a, uint64_t sym_b) const {
    uint64_t lo = std::min(sym_a, sym_b);
    uint64_t hi = std::max(sym_a, sym_b);
    auto it = correlations_.find(PairKey{lo, hi});
    if (it != correlations_.end()) {
        return it->second;
    }
    return 0.0;  // Independence assumption if not provided.
}

}  // namespace qf::risk
