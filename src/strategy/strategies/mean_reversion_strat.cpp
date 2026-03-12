#include "qf/strategy/strategies/mean_reversion_strat.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace qf::strategy {

MeanReversionStrategy::MeanReversionStrategy() = default;

std::string MeanReversionStrategy::name() {
    return "mean_reversion";
}

void MeanReversionStrategy::configure(const Config& config) {
    z_entry_ = config.get<double>("strategy.mean_reversion.z_entry", 2.0);
    z_exit_ = config.get<double>("strategy.mean_reversion.z_exit", 0.5);
    lookback_ = config.get<uint32_t>("strategy.mean_reversion.lookback", 50u);
    max_position_ = static_cast<int64_t>(
        config.get<uint32_t>("strategy.mean_reversion.max_position", 1000u));
}

void MeanReversionStrategy::set_regime_detector(signals::RegimeDetector* detector) {
    regime_detector_ = detector;
}

std::vector<Decision> MeanReversionStrategy::on_signal(
    const signals::Signal& signal,
    const PortfolioView& portfolio) {

    std::vector<Decision> decisions;

    // Only trade when regime is Ranging.
    if (regime_detector_ && !regime_detector_->is_regime(signals::MarketRegime::Ranging)) {
        return decisions;
    }

    // Extract current price from signal timestamp (same convention as other strategies).
    double current_price = price_to_double(static_cast<Price>(signal.timestamp));
    if (current_price <= 0.0) {
        return decisions;
    }

    // Current position for this symbol.
    std::string sym(signal.symbol.data, strnlen(signal.symbol.data, SYMBOL_LENGTH));
    int64_t position = portfolio.position(sym);

    uint64_t key = signal.symbol.as_key();
    double zscore = compute_zscore(key, current_price);

    // Not enough data yet to compute a meaningful z-score.
    if (price_windows_[key].size() < 2) {
        return decisions;
    }

    // --- Exit conditions: z-score crosses back toward the mean ---
    if (position > 0 && zscore > -z_exit_) {
        // Long position, price has reverted toward mean — exit.
        Decision exit_decision;
        exit_decision.symbol = signal.symbol;
        exit_decision.side = Side::Sell;
        exit_decision.quantity = static_cast<Quantity>(position);
        exit_decision.order_type = OrderType::Market;
        exit_decision.limit_price = 0;
        exit_decision.urgency = Urgency::Medium;
        decisions.push_back(exit_decision);
        return decisions;
    }

    if (position < 0 && zscore < z_exit_) {
        // Short position, price has reverted toward mean — exit.
        Decision exit_decision;
        exit_decision.symbol = signal.symbol;
        exit_decision.side = Side::Buy;
        exit_decision.quantity = static_cast<Quantity>(-position);
        exit_decision.order_type = OrderType::Market;
        exit_decision.limit_price = 0;
        exit_decision.urgency = Urgency::Medium;
        decisions.push_back(exit_decision);
        return decisions;
    }

    // --- Entry conditions: z-score exceeds thresholds ---
    if (zscore < -z_entry_ && position < max_position_) {
        // Price far below mean — buy (fade the move).
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
    } else if (zscore > z_entry_ && position > -max_position_) {
        // Price far above mean — sell (fade the move).
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

double MeanReversionStrategy::compute_zscore(uint64_t symbol_key, double current_price) {
    auto& window = price_windows_[symbol_key];
    window.push_back(current_price);

    // Trim to lookback size.
    while (window.size() > lookback_) {
        window.pop_front();
    }

    if (window.size() < 2) {
        return 0.0;
    }

    // Compute mean.
    double sum = std::accumulate(window.begin(), window.end(), 0.0);
    double mean = sum / static_cast<double>(window.size());

    // Compute standard deviation.
    double sq_sum = 0.0;
    for (double p : window) {
        double diff = p - mean;
        sq_sum += diff * diff;
    }
    double stddev = std::sqrt(sq_sum / static_cast<double>(window.size()));

    if (stddev < 1e-12) {
        return 0.0;
    }

    return (current_price - mean) / stddev;
}

}  // namespace qf::strategy
