#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "qf/common/config.hpp"
#include "qf/common/types.hpp"
#include "qf/strategy/strategy_base.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// MarketMakingStrategy — earns spread by quoting both sides of the book.
//
// The strategy quotes bid and ask prices symmetrically around a fair value
// derived from the microprice signal. Quotes are skewed based on current
// inventory to reduce risk: when long, the bid/ask are shifted down to
// encourage selling; when short, shifted up to encourage buying.
//
// Configurable parameters:
//   spread_ticks        — half-spread in ticks (1 tick = 1 price unit)
//   quote_size          — quantity per side
//   max_position        — hard inventory limit; stops quoting the side that
//                         would increase exposure beyond this
//   inventory_skew_factor — how aggressively to skew quotes per unit of
//                           inventory (in price units per contract)
//
// When the fair value moves (signal changes), the strategy cancels stale
// quotes and replaces them with new ones centered on the updated fair value.
class MarketMakingStrategy : public StrategyBase {
public:
    MarketMakingStrategy();

    std::vector<Decision> on_signal(const signals::Signal& signal,
                                    const PortfolioView& portfolio) override;

    std::string name() override;

    void configure(const Config& config) override;

    // --- Accessors (for testing) ---
    uint32_t spread_ticks() const { return spread_ticks_; }
    Quantity quote_size() const { return quote_size_; }
    int64_t  max_position() const { return max_position_; }
    double   inventory_skew_factor() const { return inventory_skew_factor_; }
    Price    last_fair_value() const { return last_fair_value_; }

private:
    // Configuration
    uint32_t spread_ticks_{2};
    Quantity quote_size_{100};
    int64_t  max_position_{1000};
    double   inventory_skew_factor_{0.5};

    // State
    Price last_fair_value_{0};

    // Compute the inventory-based skew in price units.
    // Positive inventory (long) returns a negative skew (shift quotes down).
    int64_t compute_skew(int64_t position) const;

    // Compute fair value from a signal.
    // Uses signal strength to derive a microprice-based estimate.
    Price fair_value_from_signal(const signals::Signal& signal) const;
};

}  // namespace qf::strategy
