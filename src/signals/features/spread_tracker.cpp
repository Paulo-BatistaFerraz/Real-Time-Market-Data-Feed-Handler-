#include "qf/signals/features/spread_tracker.hpp"
#include <cmath>

namespace qf::signals {

SpreadTracker::SpreadTracker(size_t window_size)
    : window_size_(window_size) {}

double SpreadTracker::compute(const core::OrderBook& book, const TradeHistory& /*trades*/) {
    auto bid = book.best_bid();
    auto ask = book.best_ask();

    // No valid spread if either side is missing
    if (!bid || !ask) {
        return 0.0;
    }

    double spread = price_to_double(*ask) - price_to_double(*bid);

    update_stats(spread);

    return zscore_;
}

std::string SpreadTracker::name() {
    return "spread_tracker";
}

void SpreadTracker::update_stats(double new_spread) {
    // Evict oldest sample if at capacity
    if (samples_.size() >= window_size_) {
        double old = samples_.front();
        samples_.pop_front();
        sum_    -= old;
        sum_sq_ -= old * old;
    }

    samples_.push_back(new_spread);
    sum_    += new_spread;
    sum_sq_ += new_spread * new_spread;

    double n = static_cast<double>(samples_.size());
    mean_ = sum_ / n;

    // Variance = E[x^2] - E[x]^2, guarded against floating-point negatives
    double variance = (sum_sq_ / n) - (mean_ * mean_);
    if (variance < 0.0) {
        variance = 0.0;
    }
    std_ = std::sqrt(variance);

    // Z-score: how many std devs the current spread is from the mean
    if (std_ > 1e-12) {
        zscore_ = (new_spread - mean_) / std_;
    } else {
        zscore_ = 0.0;
    }
}

}  // namespace qf::signals
