#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "qf/strategy/strategy_base.hpp"

namespace qf::strategy {

// StrategyRegistry — central catalog of trading strategies.
//
// Allows registering and looking up strategies by name for dynamic selection.
// Thread-safe: all accessors/mutators are guarded by a mutex.
class StrategyRegistry {
public:
    StrategyRegistry() = default;

    // Register a strategy. Overwrites any existing strategy with the same name.
    void register_strategy(const std::string& name, std::unique_ptr<StrategyBase> strategy);

    // Look up a strategy by name. Returns nullptr if not found.
    StrategyBase* get(const std::string& name) const;

    // Get all registered strategy names.
    std::vector<std::string> names() const;

    // Number of registered strategies.
    size_t count() const;

    // --- Activation ---
    // Only active strategies are evaluated by the engine.
    void set_active(const std::string& name, bool active);
    bool is_active(const std::string& name) const;
    std::vector<std::string> active_names() const;

    // Remove all registered strategies.
    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<StrategyBase>> strategies_;
    std::unordered_map<std::string, bool> active_;
};

}  // namespace qf::strategy
