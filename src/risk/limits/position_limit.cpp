#include "qf/risk/limits/position_limit.hpp"

#include <cmath>
#include <sstream>

namespace qf::risk {

PositionLimit::PositionLimit(int64_t global_max_shares)
    : global_max_(global_max_shares) {}

void PositionLimit::set_symbol_limit(const Symbol& symbol, int64_t max_shares) {
    symbol_limits_[symbol.as_key()] = max_shares;
}

RiskResult PositionLimit::check(const strategy::Decision& decision,
                                const strategy::Portfolio& portfolio) const {
    const uint64_t key = decision.symbol.as_key();
    const int64_t current_pos = portfolio.positions().get(decision.symbol);

    // Determine the signed delta: Buy adds, Sell subtracts.
    const int64_t delta = (decision.side == Side::Buy)
                              ? static_cast<int64_t>(decision.quantity)
                              : -static_cast<int64_t>(decision.quantity);

    const int64_t proposed_pos = current_pos + delta;
    const int64_t abs_proposed = std::abs(proposed_pos);
    const int64_t max_shares = limit_for(key);

    const bool passed = abs_proposed <= max_shares;

    std::ostringstream msg;
    if (!passed) {
        msg << "Position limit breached for symbol: |"
            << abs_proposed << "| > " << max_shares;
    }

    return RiskResult{
        RiskCheck::PositionLimit,
        passed,
        static_cast<double>(abs_proposed),
        static_cast<double>(max_shares),
        msg.str()
    };
}

int64_t PositionLimit::limit_for(uint64_t symbol_key) const {
    auto it = symbol_limits_.find(symbol_key);
    return (it != symbol_limits_.end()) ? it->second : global_max_;
}

}  // namespace qf::risk
