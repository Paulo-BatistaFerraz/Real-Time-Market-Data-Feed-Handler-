#pragma once

#include <cstdint>
#include <unordered_map>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// ConcentrationLimit — enforces maximum position concentration per symbol.
//
// Checks that the absolute notional exposure in any single symbol does
// not exceed |max_pct| (0–1.0) of total portfolio gross exposure.
// This prevents over-concentration in a single instrument.
class ConcentrationLimit {
public:
    // Construct with a global maximum concentration percentage (0.0–1.0).
    explicit ConcentrationLimit(double max_pct);

    // Set a per-symbol override.
    void set_symbol_limit(const Symbol& symbol, double max_pct);

    // Pre-trade check: would the proposed decision cause the symbol's
    // concentration to exceed the limit?
    // |prices| maps symbol key -> current price for notional computation.
    // |reference_price| is the price for the symbol in the decision.
    RiskResult check(const strategy::Decision& decision,
                     const strategy::Portfolio& portfolio,
                     const std::unordered_map<uint64_t, Price>& prices,
                     Price reference_price) const;

private:
    double max_pct_;
    std::unordered_map<uint64_t, double> symbol_limits_;

    double limit_for(uint64_t symbol_key) const;
};

}  // namespace qf::risk
