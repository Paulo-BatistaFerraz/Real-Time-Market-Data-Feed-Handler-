#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <deque>
#include <cstddef>

namespace qf::signals {

// Momentum feature — computes price change over short/medium/long tick windows,
// normalized by rolling standard deviation (z-score of returns).
//
// compute() returns the short-window momentum z-score.
// medium() and long_momentum() return medium/long z-scores.
// All values are 0 when insufficient data.
class Momentum : public FeatureBase {
public:
    explicit Momentum(size_t short_window = 10,
                      size_t medium_window = 50,
                      size_t long_window = 200);

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

    double short_momentum() const { return short_z_; }
    double medium_momentum() const { return medium_z_; }
    double long_momentum() const { return long_z_; }

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

    // Compute momentum z-score for a given window size.
    // Returns 0 if insufficient data.
    double compute_zscore(size_t window) const;
};

}  // namespace qf::signals
