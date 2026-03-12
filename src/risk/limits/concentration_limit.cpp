#include "qf/risk/limits/concentration_limit.hpp"

#include <cmath>
#include <sstream>

namespace qf::risk {

ConcentrationLimit::ConcentrationLimit(double max_pct)
    : max_pct_(max_pct) {}

void ConcentrationLimit::set_symbol_limit(const Symbol& symbol, double max_pct) {
    symbol_limits_[symbol.as_key()] = max_pct;
}

RiskResult ConcentrationLimit::check(
    const strategy::Decision& decision,
    const strategy::Portfolio& portfolio,
    const std::unordered_map<uint64_t, Price>& prices,
    Price reference_price) const {

    const uint64_t key = decision.symbol.as_key();
    const int64_t current_pos = portfolio.positions().get(decision.symbol);

    // Signed delta from the proposed order.
    const int64_t delta = (decision.side == Side::Buy)
                              ? static_cast<int64_t>(decision.quantity)
                              : -static_cast<int64_t>(decision.quantity);

    const int64_t proposed_pos = current_pos + delta;
    const double ref_price_dbl = price_to_double(reference_price);

    // Proposed notional for this symbol.
    const double symbol_notional = std::abs(static_cast<double>(proposed_pos)) * ref_price_dbl;

    // Compute total portfolio gross notional (using proposed position for target symbol).
    double total_notional = 0.0;
    const auto& all_positions = portfolio.positions().get_all();

    for (const auto& [sym_key, pos] : all_positions) {
        if (sym_key == key) {
            total_notional += symbol_notional;
        } else {
            auto pit = prices.find(sym_key);
            if (pit != prices.end()) {
                total_notional += std::abs(static_cast<double>(pos)) * price_to_double(pit->second);
            }
        }
    }

    // If target symbol is not yet in portfolio, add its proposed notional.
    if (all_positions.find(key) == all_positions.end()) {
        total_notional += symbol_notional;
    }

    // Compute concentration percentage.
    const double concentration = (total_notional > 0.0)
                                     ? symbol_notional / total_notional
                                     : (symbol_notional > 0.0 ? 1.0 : 0.0);

    const double max_pct = limit_for(key);
    const bool passed = concentration <= max_pct;

    std::ostringstream msg;
    if (!passed) {
        msg << "Concentration limit breached: "
            << (concentration * 100.0) << "% > "
            << (max_pct * 100.0) << "%";
    }

    return RiskResult{
        RiskCheck::ConcentrationLimit,
        passed,
        concentration,
        max_pct,
        msg.str()
    };
}

double ConcentrationLimit::limit_for(uint64_t symbol_key) const {
    auto it = symbol_limits_.find(symbol_key);
    return (it != symbol_limits_.end()) ? it->second : max_pct_;
}

}  // namespace qf::risk
