#include "qf/signals/features/momentum.hpp"
#include <cmath>

namespace qf::signals {

Momentum::Momentum(size_t short_window, size_t medium_window, size_t long_window)
    : short_window_(short_window)
    , medium_window_(medium_window)
    , long_window_(long_window) {}

double Momentum::compute(const core::OrderBook& /*book*/, const TradeHistory& trades) {
    const auto& trade_vec = trades.trades();

    // Ingest new trade prices
    for (size_t i = last_processed_; i < trade_vec.size(); ++i) {
        prices_.push_back(price_to_double(trade_vec[i].price));
    }
    last_processed_ = trade_vec.size();

    // Keep only enough prices for the largest window + 1 (need n+1 prices for n returns)
    size_t max_needed = long_window_ + 1;
    while (prices_.size() > max_needed) {
        prices_.pop_front();
    }

    short_z_  = compute_zscore(short_window_);
    medium_z_ = compute_zscore(medium_window_);
    long_z_   = compute_zscore(long_window_);

    return short_z_;
}

std::string Momentum::name() {
    return "momentum";
}

double Momentum::compute_zscore(size_t window) const {
    // Need at least window+1 prices to compute window returns
    if (prices_.size() < window + 1) {
        return 0.0;
    }

    size_t n = prices_.size();
    size_t start = n - window - 1;

    // Price change over the window
    double price_change = prices_[n - 1] - prices_[start];

    // Compute rolling returns within the window for standard deviation
    double sum = 0.0;
    double sum_sq = 0.0;
    for (size_t i = start + 1; i < n; ++i) {
        double ret = prices_[i] - prices_[i - 1];
        sum += ret;
        sum_sq += ret * ret;
    }

    double mean = sum / window;
    double variance = (sum_sq / window) - (mean * mean);
    if (variance <= 0.0) {
        return 0.0;
    }

    double stddev = std::sqrt(variance);
    return price_change / stddev;
}

}  // namespace qf::signals
