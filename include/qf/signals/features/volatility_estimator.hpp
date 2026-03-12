#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <deque>
#include <cstddef>

namespace qf::signals {

// OHLC bar constructed from trade data between compute() calls.
struct OHLCBar {
    double open;
    double high;
    double low;
    double close;
};

// Volatility Estimator feature.
// Computes realized volatility using Parkinson (high-low) and Yang-Zhang
// (open-close-high-low) estimators over a rolling window of OHLC bars.
// Each call to compute() forms one bar from new trades since the last call.
// compute() returns the Parkinson estimate; yang_zhang() returns the YZ estimate.
// Returns 0 when insufficient data.
class VolatilityEstimator : public FeatureBase {
public:
    // window_size: number of OHLC bars in the rolling window
    // annualization_factor: multiplier to annualize (e.g. 252 for daily bars)
    explicit VolatilityEstimator(size_t window_size = 20,
                                  double annualization_factor = 252.0);

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

    // Parkinson estimator: vol = sqrt(annualization / (4*n*ln2) * sum(ln(H/L)^2))
    double parkinson() const { return parkinson_vol_; }

    // Yang-Zhang estimator (open-close-high-low)
    double yang_zhang() const { return yang_zhang_vol_; }

    size_t bar_count() const { return bars_.size(); }

private:
    size_t window_size_;
    double annualization_factor_;
    size_t last_processed_ = 0;

    std::deque<OHLCBar> bars_;
    double parkinson_vol_   = 0.0;
    double yang_zhang_vol_  = 0.0;

    // Accumulator for the current bar being built
    bool   bar_started_ = false;
    double cur_open_    = 0.0;
    double cur_high_    = 0.0;
    double cur_low_     = 0.0;
    double cur_close_   = 0.0;

    void flush_bar();
    void recompute();
};

}  // namespace qf::signals
