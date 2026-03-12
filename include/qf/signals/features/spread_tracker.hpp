#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <deque>
#include <cstdint>
#include <cstddef>

namespace qf::signals {

// Spread Tracker feature.
// Tracks rolling mean, standard deviation, and z-score of the bid-ask spread.
// compute() returns the z-score of the current spread vs the rolling window:
// positive = wider than usual, negative = tighter than usual.
// Returns 0 when insufficient data or zero variance.
class SpreadTracker : public FeatureBase {
public:
    // window_size: number of spread samples to keep in the rolling window
    explicit SpreadTracker(size_t window_size = 100);

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

    // Accessors for rolling statistics
    double rolling_mean() const { return mean_; }
    double rolling_std() const { return std_; }
    double last_zscore() const { return zscore_; }
    size_t sample_count() const { return samples_.size(); }

private:
    size_t window_size_;

    std::deque<double> samples_;  // recent spread values
    double sum_      = 0.0;      // running sum for mean
    double sum_sq_   = 0.0;      // running sum of squares for variance
    double mean_     = 0.0;
    double std_      = 0.0;
    double zscore_   = 0.0;

    void update_stats(double new_spread);
};

}  // namespace qf::signals
