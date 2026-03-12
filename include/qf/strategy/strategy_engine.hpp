#pragma once

#include <functional>
#include <vector>
#include "qf/signals/signal_types.hpp"
#include "qf/strategy/strategy_registry.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// Callback type for forwarding decisions to the OMS.
using OmsCallback = std::function<void(const std::vector<Decision>&)>;

// StrategyEngine — runs active strategies, collects Decisions, forwards to OMS.
//
// The engine owns a StrategyRegistry. When a signal arrives it fans out to
// every active strategy, aggregates the resulting Decisions, and delivers
// them to the OMS via a registered callback.
class StrategyEngine {
public:
    StrategyEngine();

    // Access the registry for registration and configuration.
    StrategyRegistry& registry() { return registry_; }
    const StrategyRegistry& registry() const { return registry_; }

    // Set the callback that receives collected decisions.
    void set_oms_callback(OmsCallback callback);

    // Process a signal through all active strategies.
    // Returns the aggregated decisions (also forwarded to OMS if callback set).
    std::vector<Decision> on_signal(const signals::Signal& signal,
                                    const PortfolioView& portfolio);

    // Enable a strategy by name (sets it active in the registry).
    void enable(const std::string& name);

    // Disable a strategy by name (sets it inactive in the registry).
    void disable(const std::string& name);

    // Update configuration for a named strategy.
    void configure(const std::string& name, const Config& config);

    // Get decisions from the most recent on_signal() call.
    const std::vector<Decision>& last_decisions() const { return last_decisions_; }

    // Reset engine state.
    void reset();

private:
    StrategyRegistry registry_;
    OmsCallback oms_callback_;
    std::vector<Decision> last_decisions_;
};

}  // namespace qf::strategy
