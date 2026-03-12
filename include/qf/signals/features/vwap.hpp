#pragma once

#include "qf/signals/features/feature_base.hpp"

namespace qf::signals {

// Volume-Weighted Average Price (VWAP) feature.
// Tracks cumulative (price * quantity) and cumulative quantity from trades.
// compute() returns sum(price*qty) / sum(qty).
// reset() clears accumulators for daily reset.
class VWAP : public FeatureBase {
public:
    VWAP() = default;

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

    // Clear accumulators for daily reset
    void reset();

private:
    double cum_price_qty_ = 0.0;  // sum(price * quantity)
    double cum_qty_       = 0.0;  // sum(quantity)
    size_t last_processed_ = 0;   // index of last processed trade in history
};

}  // namespace qf::signals
