#include "qf/strategy/strategy_registry.hpp"

namespace qf::strategy {

void StrategyRegistry::register_strategy(const std::string& name,
                                          std::unique_ptr<StrategyBase> strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    strategies_[name] = std::move(strategy);
    active_[name] = true;  // active by default
}

StrategyBase* StrategyRegistry::get(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = strategies_.find(name);
    return (it != strategies_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> StrategyRegistry::names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(strategies_.size());
    for (const auto& [name, _] : strategies_) {
        result.push_back(name);
    }
    return result;
}

size_t StrategyRegistry::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return strategies_.size();
}

void StrategyRegistry::set_active(const std::string& name, bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_[name] = active;
}

bool StrategyRegistry::is_active(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_.find(name);
    return (it != active_.end()) ? it->second : false;
}

std::vector<std::string> StrategyRegistry::active_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [name, active] : active_) {
        if (active && strategies_.count(name)) {
            result.push_back(name);
        }
    }
    return result;
}

void StrategyRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    strategies_.clear();
    active_.clear();
}

}  // namespace qf::strategy
