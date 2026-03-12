#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <deque>
#include <cstddef>

namespace qf::signals {

// Mean Reversion feature — computes the z-score of the current price relative
// to a rolling moving average over short/medium/long tick windows.
//
// Positive z-score = price above mean (potential sell signal).
// Negative z-score = price below mean (potential buy signal).
//
// compute() returns the short-window z-score.
// medium() and long_reversion() return medium/long z-scores.
// All values are 0 when insufficient data.
class MeanReversion : public FeatureBase {
public:
    explicit MeanReversion(size_t short_window = 10,
                           size_t medium_window = 50,
                           size_t long_window = 200);

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

    double short_zscore() const { return short_z_; }
    double medium_zscore() const { return medium_z_; }
    double long_zscore() const { return long_z_; }

    size_t price_count() const { return prices_.size(); }

private:
    size_t short_window_;
    size_t medium_window_;
    size_t long_window_;
    size_t last_processed_ = 0;

    std::deque<double> prices_;  // rolling price history

    double short_z_  = 0.0;
    double medium_z_ = 0.0;
    double long_z_   = 0.0;

    // Compute z-score of current price vs rolling moving average for a given window.
    double compute_zscore(size_t window) const;
};

}  // namespace qf::signals
