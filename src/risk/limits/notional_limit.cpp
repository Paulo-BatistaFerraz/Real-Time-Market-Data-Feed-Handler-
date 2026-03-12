#include "qf/risk/limits/notional_limit.hpp"

#include <cmath>
#include <sstream>

namespace qf::risk {

NotionalLimit::NotionalLimit(double global_max_dollars)
    : global_max_(global_max_dollars) {}

void NotionalLimit::set_symbol_limit(const Symbol& symbol, double max_dollars) {
    symbol_limits_[symbol.as_key()] = max_dollars;
}

RiskResult NotionalLimit::check(const strategy::Decision& decision,
                                const strategy::Portfolio& portfolio,
                                Price reference_price) const {
    const uint64_t key = decision.symbol.as_key();
    const int64_t current_pos = portfolio.positions().get(decision.symbol);

    // Signed delta.
    const int64_t delta = (decision.side == Side::Buy)
                              ? static_cast<int64_t>(decision.quantity)
                              : -static_cast<int64_t>(decision.quantity);

    const double price_dbl = price_to_double(reference_price);

    // Current gross exposure for this symbol.
    const double current_notional = std::abs(static_cast<double>(current_pos)) * price_dbl;

    // Proposed notional after the order.
    const int64_t proposed_pos = current_pos + delta;
    const double proposed_notional = std::abs(static_cast<double>(proposed_pos)) * price_dbl;

    // Also compute the order's own notional contribution.
    const double order_notional = static_cast<double>(decision.quantity) * price_dbl;

    // Compute gross exposure across all symbols (sum of absolute notionals).
    double total_gross = 0.0;
    const auto& all_positions = portfolio.positions().get_all();
    for (const auto& [sym_key, pos] : all_positions) {
        if (sym_key == key) {
            // Use proposed position for this symbol.
            total_gross += std::abs(static_cast<double>(proposed_pos)) * price_dbl;
        } else {
            // For other symbols we'd need their prices; use position count as
            // a conservative proxy (the caller should use compute_exposure for
            // accurate cross-symbol totals).  Here we focus on per-symbol check.
        }
    }

    // Per-symbol limit check.
    const double max_dollars = limit_for(key);
    const bool passed = proposed_notional <= max_dollars;

    std::ostringstream msg;
    if (!passed) {
        msg << "Notional limit breached: $" << proposed_notional
            << " > $" << max_dollars;
    }

    return RiskResult{
        RiskCheck::NotionalLimit,
        passed,
        proposed_notional,
        max_dollars,
        msg.str()
    };
}

Exposure NotionalLimit::compute_exposure(
    const strategy::Portfolio& portfolio,
    const std::unordered_map<uint64_t, Price>& prices) const {

    Exposure exp;

    const auto& all_positions = portfolio.positions().get_all();
    for (const auto& [sym_key, pos] : all_positions) {
        auto pit = prices.find(sym_key);
        if (pit == prices.end()) continue;

        const double price_dbl = price_to_double(pit->second);
        const double signed_notional = static_cast<double>(pos) * price_dbl;

        exp.per_symbol[sym_key] = signed_notional;
        exp.net_exposure += signed_notional;
        exp.gross_exposure += std::abs(signed_notional);
    }

    return exp;
}

double NotionalLimit::limit_for(uint64_t symbol_key) const {
    auto it = symbol_limits_.find(symbol_key);
    return (it != symbol_limits_.end()) ? it->second : global_max_;
}

}  // namespace qf::risk
