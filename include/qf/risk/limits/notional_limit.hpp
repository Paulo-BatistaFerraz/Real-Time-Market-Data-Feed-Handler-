#pragma once

#include <cstdint>
#include <unordered_map>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// NotionalLimit — enforces maximum dollar exposure.
//
// Tracks net and gross exposure across all positions.  Before a new order,
// checks that current_exposure + (order_qty * price) <= max_dollars for
// both per-symbol and global limits.
class NotionalLimit {
public:
    // Construct with a global maximum notional exposure (dollars).
    explicit NotionalLimit(double global_max_dollars);

    // Set a per-symbol notional override.
    void set_symbol_limit(const Symbol& symbol, double max_dollars);

    // Pre-trade check: would the proposed decision breach limits?
    // Uses limit_price for limit orders, or last_price for market orders.
    RiskResult check(const strategy::Decision& decision,
                     const strategy::Portfolio& portfolio,
                     Price reference_price) const;

    // Compute current net and gross exposure from portfolio state.
    // |prices| maps symbol key -> current price (fixed-point).
    Exposure compute_exposure(
        const strategy::Portfolio& portfolio,
        const std::unordered_map<uint64_t, Price>& prices) const;

private:
    double global_max_;
    std::unordered_map<uint64_t, double> symbol_limits_;

    double limit_for(uint64_t symbol_key) const;
};

}  // namespace qf::risk
