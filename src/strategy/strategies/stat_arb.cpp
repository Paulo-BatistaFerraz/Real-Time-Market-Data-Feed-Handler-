#include "qf/strategy/strategies/stat_arb.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace qf::strategy {

StatArbStrategy::StatArbStrategy() = default;

std::string StatArbStrategy::name() {
    return "stat_arb";
}

void StatArbStrategy::configure(const Config& config) {
    symbol_a_ = config.get<std::string>("strategy.stat_arb.symbol_a", std::string("AAPL"));
    symbol_b_ = config.get<std::string>("strategy.stat_arb.symbol_b", std::string("MSFT"));
    entry_zscore_ = config.get<double>("strategy.stat_arb.entry_zscore", 2.0);
    exit_zscore_ = config.get<double>("strategy.stat_arb.exit_zscore", 0.5);
    lookback_ = config.get<uint32_t>("strategy.stat_arb.lookback", 50u);
    trade_size_ = static_cast<Quantity>(
        config.get<uint32_t>("strategy.stat_arb.trade_size", 100u));
}

bool StatArbStrategy::is_symbol_a(const signals::Signal& signal) const {
    std::string sym(signal.symbol.data, strnlen(signal.symbol.data, SYMBOL_LENGTH));
    return sym == symbol_a_;
}

bool StatArbStrategy::is_symbol_b(const signals::Signal& signal) const {
    std::string sym(signal.symbol.data, strnlen(signal.symbol.data, SYMBOL_LENGTH));
    return sym == symbol_b_;
}

std::vector<Decision> StatArbStrategy::on_signal(
    const signals::Signal& signal,
    const PortfolioView& portfolio) {

    std::vector<Decision> decisions;

    // Extract current price from signal timestamp (same convention as other strategies).
    double current_price = price_to_double(static_cast<Price>(signal.timestamp));
    if (current_price <= 0.0) {
        return decisions;
    }

    // Update last observed price for the relevant leg.
    bool updated_ratio = false;
    if (is_symbol_a(signal)) {
        last_price_a_ = current_price;
        // Only update the ratio window when we have both prices.
        if (last_price_b_ > 0.0) {
            updated_ratio = true;
        }
    } else if (is_symbol_b(signal)) {
        last_price_b_ = current_price;
        if (last_price_a_ > 0.0) {
            updated_ratio = true;
        }
    } else {
        // Signal is for an unrelated symbol — ignore.
        return decisions;
    }

    // Need both prices before we can compute anything.
    if (!updated_ratio || last_price_a_ <= 0.0 || last_price_b_ <= 0.0) {
        return decisions;
    }

    double current_ratio = last_price_a_ / last_price_b_;

    // Add to rolling window.
    ratio_window_.push_back(current_ratio);
    while (ratio_window_.size() > lookback_) {
        ratio_window_.pop_front();
    }

    // Need enough history.
    if (ratio_window_.size() < 2) {
        return decisions;
    }

    double zscore = compute_ratio_zscore(current_ratio);

    Symbol sym_a(symbol_a_.c_str());
    Symbol sym_b(symbol_b_.c_str());

    // --- Exit conditions ---
    if (spread_position_ != 0 && std::abs(zscore) < exit_zscore_) {
        // Spread has reverted — close both legs.
        if (spread_position_ > 0) {
            // Was long A / short B — sell A, buy B.
            Decision sell_a;
            sell_a.symbol = sym_a;
            sell_a.side = Side::Sell;
            sell_a.quantity = trade_size_;
            sell_a.order_type = OrderType::Market;
            sell_a.limit_price = 0;
            sell_a.urgency = Urgency::Medium;
            decisions.push_back(sell_a);

            Decision buy_b;
            buy_b.symbol = sym_b;
            buy_b.side = Side::Buy;
            buy_b.quantity = trade_size_;
            buy_b.order_type = OrderType::Market;
            buy_b.limit_price = 0;
            buy_b.urgency = Urgency::Medium;
            decisions.push_back(buy_b);
        } else {
            // Was short A / long B — buy A, sell B.
            Decision buy_a;
            buy_a.symbol = sym_a;
            buy_a.side = Side::Buy;
            buy_a.quantity = trade_size_;
            buy_a.order_type = OrderType::Market;
            buy_a.limit_price = 0;
            buy_a.urgency = Urgency::Medium;
            decisions.push_back(buy_a);

            Decision sell_b;
            sell_b.symbol = sym_b;
            sell_b.side = Side::Sell;
            sell_b.quantity = trade_size_;
            sell_b.order_type = OrderType::Market;
            sell_b.limit_price = 0;
            sell_b.urgency = Urgency::Medium;
            decisions.push_back(sell_b);
        }
        spread_position_ = 0;
        return decisions;
    }

    // --- Entry conditions (only when flat) ---
    if (spread_position_ == 0) {
        if (zscore < -entry_zscore_) {
            // Ratio is below mean: A is cheap relative to B.
            // Long A (cheap) / Short B (expensive).
            Decision buy_a;
            buy_a.symbol = sym_a;
            buy_a.side = Side::Buy;
            buy_a.quantity = trade_size_;
            buy_a.order_type = OrderType::Market;
            buy_a.limit_price = 0;
            buy_a.urgency = Urgency::Medium;
            decisions.push_back(buy_a);

            Decision sell_b;
            sell_b.symbol = sym_b;
            sell_b.side = Side::Sell;
            sell_b.quantity = trade_size_;
            sell_b.order_type = OrderType::Market;
            sell_b.limit_price = 0;
            sell_b.urgency = Urgency::Medium;
            decisions.push_back(sell_b);

            spread_position_ = 1;  // long A / short B
        } else if (zscore > entry_zscore_) {
            // Ratio is above mean: A is expensive relative to B.
            // Short A (expensive) / Long B (cheap).
            Decision sell_a;
            sell_a.symbol = sym_a;
            sell_a.side = Side::Sell;
            sell_a.quantity = trade_size_;
            sell_a.order_type = OrderType::Market;
            sell_a.limit_price = 0;
            sell_a.urgency = Urgency::Medium;
            decisions.push_back(sell_a);

            Decision buy_b;
            buy_b.symbol = sym_b;
            buy_b.side = Side::Buy;
            buy_b.quantity = trade_size_;
            buy_b.order_type = OrderType::Market;
            buy_b.limit_price = 0;
            buy_b.urgency = Urgency::Medium;
            decisions.push_back(buy_b);

            spread_position_ = -1;  // short A / long B
        }
    }

    return decisions;
}

double StatArbStrategy::compute_ratio_zscore(double current_ratio) const {
    if (ratio_window_.size() < 2) {
        return 0.0;
    }

    double sum = std::accumulate(ratio_window_.begin(), ratio_window_.end(), 0.0);
    double mean = sum / static_cast<double>(ratio_window_.size());

    double sq_sum = 0.0;
    for (double r : ratio_window_) {
        double diff = r - mean;
        sq_sum += diff * diff;
    }
    double stddev = std::sqrt(sq_sum / static_cast<double>(ratio_window_.size()));

    if (stddev < 1e-12) {
        return 0.0;
    }

    return (current_ratio - mean) / stddev;
}

}  // namespace qf::strategy
