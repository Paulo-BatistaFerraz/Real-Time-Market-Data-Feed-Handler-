#include "qf/strategy/strategies/momentum_follower.hpp"

#include <algorithm>
#include <cmath>

namespace qf::strategy {

MomentumFollowerStrategy::MomentumFollowerStrategy() = default;

std::string MomentumFollowerStrategy::name() {
    return "momentum_follower";
}

void MomentumFollowerStrategy::configure(const Config& config) {
    entry_threshold_ = config.get<double>("strategy.momentum_follower.entry_threshold", 0.5);
    exit_threshold_ = config.get<double>("strategy.momentum_follower.exit_threshold", 0.3);
    lookback_period_ = config.get<uint32_t>("strategy.momentum_follower.lookback_period", 20u);
    max_position_ = static_cast<int64_t>(
        config.get<uint32_t>("strategy.momentum_follower.max_position", 1000u));
    trailing_stop_pct_ = config.get<double>("strategy.momentum_follower.trailing_stop_pct", 0.02);
}

void MomentumFollowerStrategy::set_regime_detector(signals::RegimeDetector* detector) {
    regime_detector_ = detector;
}

std::vector<Decision> MomentumFollowerStrategy::on_signal(
    const signals::Signal& signal,
    const PortfolioView& portfolio) {

    std::vector<Decision> decisions;

    // Only trade when regime is Trending.
    if (regime_detector_ && !regime_detector_->is_regime(signals::MarketRegime::Trending)) {
        return decisions;
    }

    // Extract current price from signal timestamp (same convention as market_making).
    double current_price = price_to_double(static_cast<Price>(signal.timestamp));
    if (current_price <= 0.0) {
        return decisions;
    }

    // Current position for this symbol.
    std::string sym(signal.symbol.data, strnlen(signal.symbol.data, SYMBOL_LENGTH));
    int64_t position = portfolio.position(sym);

    double momentum = signal.strength;  // [-1.0, +1.0]

    // --- Check trailing stop first (exit before considering new entries) ---
    if (position != 0) {
        update_trailing_stop(signal.symbol, current_price, position);

        if (trailing_stop_triggered(signal.symbol, current_price, position)) {
            // Exit entire position via market order.
            Decision exit_decision;
            exit_decision.symbol = signal.symbol;
            exit_decision.side = (position > 0) ? Side::Sell : Side::Buy;
            exit_decision.quantity = static_cast<Quantity>(std::abs(position));
            exit_decision.order_type = OrderType::Market;
            exit_decision.limit_price = 0;
            exit_decision.urgency = Urgency::High;
            decisions.push_back(exit_decision);
            reset_trailing_stop(signal.symbol);
            return decisions;
        }
    }

    // --- Check exit conditions (momentum crosses below negative threshold) ---
    if (position > 0 && momentum < -exit_threshold_) {
        // Long position, momentum turned bearish — exit.
        Decision exit_decision;
        exit_decision.symbol = signal.symbol;
        exit_decision.side = Side::Sell;
        exit_decision.quantity = static_cast<Quantity>(position);
        exit_decision.order_type = OrderType::Market;
        exit_decision.limit_price = 0;
        exit_decision.urgency = Urgency::High;
        decisions.push_back(exit_decision);
        reset_trailing_stop(signal.symbol);
        return decisions;
    }

    if (position < 0 && momentum > exit_threshold_) {
        // Short position, momentum turned bullish — exit.
        Decision exit_decision;
        exit_decision.symbol = signal.symbol;
        exit_decision.side = Side::Buy;
        exit_decision.quantity = static_cast<Quantity>(-position);
        exit_decision.order_type = OrderType::Market;
        exit_decision.limit_price = 0;
        exit_decision.urgency = Urgency::High;
        decisions.push_back(exit_decision);
        reset_trailing_stop(signal.symbol);
        return decisions;
    }

    // --- Check entry conditions (momentum crosses above threshold) ---
    if (momentum > entry_threshold_ && position < max_position_) {
        // Strong bullish momentum — buy.
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

            // Initialize trailing stop for new entry.
            auto& stop = trailing_stops_[signal.symbol.as_key()];
            if (!stop.active) {
                stop.peak_price = current_price;
                stop.active = true;
            }
        }
    } else if (momentum < -entry_threshold_ && position > -max_position_) {
        // Strong bearish momentum — sell.
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

            // Initialize trailing stop for new entry.
            auto& stop = trailing_stops_[signal.symbol.as_key()];
            if (!stop.active) {
                stop.trough_price = current_price;
                stop.active = true;
            }
        }
    }

    return decisions;
}

bool MomentumFollowerStrategy::trailing_stop_triggered(
    const Symbol& symbol, double current_price, int64_t position) {

    auto it = trailing_stops_.find(symbol.as_key());
    if (it == trailing_stops_.end() || !it->second.active) {
        return false;
    }

    const auto& stop = it->second;

    if (position > 0) {
        // Long: trigger if price dropped trailing_stop_pct_ from peak.
        double stop_level = stop.peak_price * (1.0 - trailing_stop_pct_);
        return current_price <= stop_level;
    } else if (position < 0) {
        // Short: trigger if price rose trailing_stop_pct_ from trough.
        double stop_level = stop.trough_price * (1.0 + trailing_stop_pct_);
        return current_price >= stop_level;
    }

    return false;
}

void MomentumFollowerStrategy::update_trailing_stop(
    const Symbol& symbol, double current_price, int64_t position) {

    auto& stop = trailing_stops_[symbol.as_key()];
    if (!stop.active) {
        // First update — initialize.
        stop.active = true;
        stop.peak_price = current_price;
        stop.trough_price = current_price;
        return;
    }

    if (position > 0) {
        // Long: track highest price.
        stop.peak_price = std::max(stop.peak_price, current_price);
    } else if (position < 0) {
        // Short: track lowest price.
        if (stop.trough_price <= 0.0) {
            stop.trough_price = current_price;
        } else {
            stop.trough_price = std::min(stop.trough_price, current_price);
        }
    }
}

void MomentumFollowerStrategy::reset_trailing_stop(const Symbol& symbol) {
    trailing_stops_.erase(symbol.as_key());
}

}  // namespace qf::strategy
