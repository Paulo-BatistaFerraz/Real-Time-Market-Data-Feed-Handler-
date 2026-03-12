#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <cstddef>

namespace qf::signals {

// Book Depth Ratio feature.
// Computes total bid depth / total ask depth across the top N price levels.
// Returns ratio > 1 when more bid depth (bullish), < 1 when more ask depth.
// Returns 0 when both sides are empty, or NaN when only ask side is empty.
class BookDepthRatio : public FeatureBase {
public:
    // num_levels: number of price levels to consider on each side (default 5)
    explicit BookDepthRatio(size_t num_levels = 5);

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

private:
    size_t num_levels_;
};

}  // namespace qf::signals
