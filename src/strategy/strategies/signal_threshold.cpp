#include "qf/strategy/strategies/signal_threshold.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace qf::strategy {

SignalThresholdStrategy::SignalThresholdStrategy() = default;

std::string SignalThresholdStrategy::name() {
    return "signal_threshold";
}

void SignalThresholdStrategy::configure(const Config& config) {
    signal_name_ = config.get<std::string>("strategy.signal_threshold.signal_name", std::string("momentum"));
    buy_threshold_ = config.get<double>("strategy.signal_threshold.buy_threshold", 0.5);
    sell_threshold_ = config.get<double>("strategy.signal_threshold.sell_threshold", -0.5);
    exit_threshold_ = config.get<double>("strategy.signal_threshold.exit_threshold", 0.0);
    max_position_ = static_cast<int64_t>(
        config.get<uint32_t>("strategy.signal_threshold.max_position", 1000u));
}

bool SignalThresholdStrategy::signal_type_from_name(const std::string& name,
                                                     signals::SignalType& out) {
    if (name == "momentum")       { out = signals::SignalType::Momentum;      return true; }
    if (name == "mean_reversion") { out = signals::SignalType::MeanReversion; return true; }
    if (name == "order_flow")     { out = signals::SignalType::OrderFlow;     return true; }
    if (name == "volatility")     { out = signals::SignalType::Volatility;    return true; }
    if (name == "composite")      { out = signals::SignalType::Composite;     return true; }
    if (name == "ml")             { out = signals::SignalType::ML;            return true; }
    return false;
}

bool SignalThresholdStrategy::matches_signal(const signals::Signal& signal) const {
    signals::SignalType expected;
    if (!signal_type_from_name(signal_name_, expected)) {
        // If the configured name doesn't map to any known type, accept all signals
        // (generic fallback — works with any registered signal).
        return true;
    }
    return signal.type == expected;
}

std::vector<Decision> SignalThresholdStrategy::on_signal(
    const signals::Signal& signal,
    const PortfolioView& portfolio) {

    std::vector<Decision> decisions;

    // Only react to the configured signal type.
    if (!matches_signal(signal)) {
        return decisions;
    }

    // Current position for this symbol.
    std::string sym(signal.symbol.data, strnlen(signal.symbol.data, SYMBOL_LENGTH));
    int64_t position = portfolio.position(sym);

    double strength = signal.strength;  // [-1.0, +1.0]

    // --- Exit conditions: signal crosses back through exit_threshold ---
    if (position > 0 && strength <= exit_threshold_) {
        // Long position, signal crossed below exit_threshold — exit.
        Decision exit_decision;
        exit_decision.symbol = signal.symbol;
        exit_decision.side = Side::Sell;
        exit_decision.quantity = static_cast<Quantity>(position);
        exit_decision.order_type = OrderType::Market;
        exit_decision.limit_price = 0;
        exit_decision.urgency = Urgency::High;
        decisions.push_back(exit_decision);
        return decisions;
    }

    if (position < 0 && strength >= -exit_threshold_) {
        // Short position, signal crossed above -exit_threshold — exit.
        Decision exit_decision;
        exit_decision.symbol = signal.symbol;
        exit_decision.side = Side::Buy;
        exit_decision.quantity = static_cast<Quantity>(-position);
        exit_decision.order_type = OrderType::Market;
        exit_decision.limit_price = 0;
        exit_decision.urgency = Urgency::High;
        decisions.push_back(exit_decision);
        return decisions;
    }

    // --- Entry conditions ---
    if (strength > buy_threshold_ && position < max_position_) {
        // Signal above buy_threshold — buy.
        Quantity qty = static_cast<Quantity>(
            std::min<int64_t>(max_position_ - position, max_position_));
        if (qty > 0) {
            Decision buy_decision;
            buy_decision.symbol = signal.symbol;
            buy_decision.side = Side::Buy;
            buy_decision.quantity = qty;
            buy_decision.order_type = OrderType::Market;
            buy_decision.limit_price = 0;
            buy_decision.urgency = Urgency::Medium;
            decisions.push_back(buy_decision);
        }
    } else if (strength < sell_threshold_ && position > -max_position_) {
        // Signal below sell_threshold — sell.
        Quantity qty = static_cast<Quantity>(
            std::min<int64_t>(max_position_ + position, max_position_));
        if (qty > 0) {
            Decision sell_decision;
            sell_decision.symbol = signal.symbol;
            sell_decision.side = Side::Sell;
            sell_decision.quantity = qty;
            sell_decision.order_type = OrderType::Market;
            sell_decision.limit_price = 0;
            sell_decision.urgency = Urgency::Medium;
            decisions.push_back(sell_decision);
        }
    }

    return decisions;
}

}  // namespace qf::strategy
