#pragma once

#include <string>
#include <vector>
#include "qf/common/config.hpp"
#include "qf/signals/signal_types.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// StrategyBase — abstract interface for all trading strategies.
//
// Each strategy receives signals and the current portfolio, then returns
// zero or more trading decisions. Strategies are registered by name in
// the StrategyRegistry and driven by the StrategyEngine.
class StrategyBase {
public:
    virtual ~StrategyBase() = default;

    // Process a signal and return trading decisions.
    virtual std::vector<Decision> on_signal(const signals::Signal& signal,
                                            const PortfolioView& portfolio) = 0;

    // Human-readable strategy name (must be unique within the registry).
    virtual std::string name() = 0;

    // Apply configuration parameters to the strategy.
    virtual void configure(const Config& config) = 0;
};

}  // namespace qf::strategy
