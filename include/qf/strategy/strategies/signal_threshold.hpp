#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "qf/common/config.hpp"
#include "qf/common/types.hpp"
#include "qf/strategy/strategy_base.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// SignalThresholdStrategy — generic threshold-based strategy that drives
// trades from any signal using configurable buy/sell/exit thresholds.
//
// The strategy is signal-agnostic: it filters incoming signals by a
// configurable signal_name (matched against SignalType) and applies
// simple threshold logic:
//   - Buys when signal.strength > buy_threshold
//   - Sells when signal.strength < sell_threshold (sell_threshold is negative)
//   - Exits a long when signal.strength crosses below exit_threshold
//   - Exits a short when signal.strength crosses above -exit_threshold
//
// Configurable parameters:
//   signal_name     — the signal type name to react to (default "momentum")
//   buy_threshold   — signal strength above which to buy (default 0.5)
//   sell_threshold  — signal strength below which to sell (default -0.5)
//   exit_threshold  — signal strength at which to exit positions (default 0.0)
//   max_position    — maximum position size in either direction (default 1000)
class SignalThresholdStrategy : public StrategyBase {
public:
    SignalThresholdStrategy();

    std::vector<Decision> on_signal(const signals::Signal& signal,
                                    const PortfolioView& portfolio) override;

    std::string name() override;

    void configure(const Config& config) override;

    // --- Accessors (for testing) ---
    const std::string& signal_name() const { return signal_name_; }
    double   buy_threshold() const { return buy_threshold_; }
    double   sell_threshold() const { return sell_threshold_; }
    double   exit_threshold() const { return exit_threshold_; }
    int64_t  max_position() const { return max_position_; }

private:
    // Configuration
    std::string signal_name_{"momentum"};
    double   buy_threshold_{0.5};
    double   sell_threshold_{-0.5};
    double   exit_threshold_{0.0};
    int64_t  max_position_{1000};

    // Map a signal type name string to the SignalType enum.
    // Returns true if the mapping was found, false otherwise.
    static bool signal_type_from_name(const std::string& name,
                                      signals::SignalType& out);

    // Check if the incoming signal matches the configured signal name.
    bool matches_signal(const signals::Signal& signal) const;
};

}  // namespace qf::strategy
