#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "qf/common/config.hpp"
#include "qf/common/types.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// Sizing method used by OrderSizer.
enum class SizingMethod : uint8_t {
    Fixed           = 0,   // constant lot size
    FixedFractional = 1,   // percentage of portfolio cash
    Kelly           = 2    // Kelly criterion
};

// Risk parameters consumed by the Kelly sizer.
struct RiskBudget {
    double win_rate{0.55};         // P(win), e.g. 0.55
    double win_loss_ratio{1.5};    // avg_win / avg_loss
    double max_portfolio_risk{0.1}; // max fraction of cash risked per trade
};

// OrderSizer — computes trade quantity from signal strength, portfolio
// state, and risk constraints.
//
// Supports three sizing methods:
//   Fixed           — always returns the configured fixed size, scaled by
//                     |signal_strength|.
//   FixedFractional — sizes as a percentage of available cash, scaled by
//                     |signal_strength|.
//   Kelly           — applies the Kelly criterion (f* = p - q/b) scaled by
//                     |signal_strength|, capped by max_portfolio_risk.
//
// All methods respect the configurable max_position limit.
//
// Thread-safety: const methods are safe for concurrent reads; mutators
// are not thread-safe and must be synchronised externally.
class OrderSizer {
public:
    explicit OrderSizer(SizingMethod method = SizingMethod::Fixed);

    // --- Setters -----------------------------------------------------------

    void set_fixed_size(Quantity size);
    void set_fractional_pct(double pct);   // e.g. 0.02 = 2 %
    void set_max_position(Quantity max_pos);

    // --- Core API ----------------------------------------------------------

    // Compute the order quantity.
    //   signal_strength : [-1, +1] from signal engine (sign = direction)
    //   portfolio       : current portfolio snapshot (cash, positions)
    //   risk_budget     : Kelly parameters and risk caps
    // Returns 0 when the signal is too weak or limits prevent trading.
    Quantity size(double signal_strength,
                  const PortfolioView& portfolio,
                  const RiskBudget& risk_budget) const;

    // --- Accessors ---------------------------------------------------------

    SizingMethod method() const { return method_; }
    Quantity max_position() const { return max_position_; }

    // --- Configuration -----------------------------------------------------

    void configure(const Config& config);

private:
    SizingMethod method_;
    Quantity fixed_size_{100};
    double  fractional_pct_{0.02};
    Quantity max_position_{10000};

    Quantity size_fixed(double abs_strength) const;
    Quantity size_fractional(double abs_strength,
                            const PortfolioView& portfolio) const;
    Quantity size_kelly(double abs_strength,
                        const PortfolioView& portfolio,
                        const RiskBudget& budget) const;
    Quantity apply_limits(Quantity raw_qty) const;
};

}  // namespace qf::strategy
