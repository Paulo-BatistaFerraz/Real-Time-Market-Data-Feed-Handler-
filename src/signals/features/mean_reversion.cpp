#include "qf/signals/features/mean_reversion.hpp"
#include <cmath>

namespace qf::signals {

MeanReversion::MeanReversion(size_t short_window, size_t medium_window, size_t long_window)
    : short_window_(short_window)
    , medium_window_(medium_window)
    , long_window_(long_window) {}

double MeanReversion::compute(const core::OrderBook& /*book*/, const TradeHistory& trades) {
    const auto& trade_vec = trades.trades();

    // Ingest new trade prices
    for (size_t i = last_processed_; i < trade_vec.size(); ++i) {
        prices_.push_back(price_to_double(trade_vec[i].price));
    }
    last_processed_ = trade_vec.size();

    // Keep only enough prices for the largest window
    while (prices_.size() > long_window_) {
        prices_.pop_front();
    }

    short_z_  = compute_zscore(short_window_);
    medium_z_ = compute_zscore(medium_window_);
    long_z_   = compute_zscore(long_window_);

    return short_z_;
}

std::string MeanReversion::name() {
    return "mean_reversion";
}

double MeanReversion::compute_zscore(size_t window) const {
    if (prices_.size() < window) {
        return 0.0;
    }

    size_t n = prices_.size();
    size_t start = n - window;

    // Compute rolling moving average over the window
    double sum = 0.0;
    for (size_t i = start; i < n; ++i) {
        sum += prices_[i];
    }
    double mean = sum / window;

    // Compute standard deviation of prices in the window
    double sum_sq = 0.0;
    for (size_t i = start; i < n; ++i) {
        double diff = prices_[i] - mean;
        sum_sq += diff * diff;
    }
    double variance = sum_sq / window;
    if (variance <= 0.0) {
        return 0.0;
    }

    double stddev = std::sqrt(variance);
    double current_price = prices_[n - 1];

    // z-score: how far current price is from the mean, in units of std dev
    // Positive = above mean (potential sell), Negative = below mean (potential buy)
    return (current_price - mean) / stddev;
}

}  // namespace qf::signals
