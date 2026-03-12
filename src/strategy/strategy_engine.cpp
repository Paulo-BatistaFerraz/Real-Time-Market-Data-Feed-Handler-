#include "qf/strategy/strategy_engine.hpp"

namespace qf::strategy {

StrategyEngine::StrategyEngine() = default;

void StrategyEngine::set_oms_callback(OmsCallback callback) {
    oms_callback_ = std::move(callback);
}

std::vector<Decision> StrategyEngine::on_signal(const signals::Signal& signal,
                                                 const PortfolioView& portfolio) {
    last_decisions_.clear();

    auto active = registry_.active_names();
    for (const auto& name : active) {
        auto* strat = registry_.get(name);
        if (!strat) continue;

        auto decisions = strat->on_signal(signal, portfolio);
        last_decisions_.insert(last_decisions_.end(), decisions.begin(), decisions.end());
    }

    if (oms_callback_ && !last_decisions_.empty()) {
        oms_callback_(last_decisions_);
    }

    return last_decisions_;
}

void StrategyEngine::enable(const std::string& name) {
    registry_.set_active(name, true);
}

void StrategyEngine::disable(const std::string& name) {
    registry_.set_active(name, false);
}

void StrategyEngine::configure(const std::string& name, const Config& config) {
    auto* strat = registry_.get(name);
    if (strat) {
        strat->configure(config);
    }
}

void StrategyEngine::reset() {
    registry_.clear();
    last_decisions_.clear();
    oms_callback_ = nullptr;
}

}  // namespace qf::strategy
