#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <deque>
#include <cstdint>
#include <cstddef>

namespace qf::signals {

// Trade Flow feature.
// Detects aggressor side by comparing trade price to the mid-price at time of trade.
// Tracks ratio of buy-aggressor vs sell-aggressor trades over a rolling window.
// compute() returns (buy_count - sell_count) / total_count, in [-1, +1].
// +1 = all buy-aggressor, -1 = all sell-aggressor, 0 = balanced.
class TradeFlow : public FeatureBase {
public:
    // window_size: number of trades to keep in the rolling window
    explicit TradeFlow(size_t window_size = 200);

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

    // Accessors
    double buy_ratio() const;
    double sell_ratio() const;
    size_t buy_count() const { return buy_count_; }
    size_t sell_count() const { return sell_count_; }
    size_t sample_count() const { return samples_.size(); }

private:
    size_t window_size_;

    enum class AggressorSide : uint8_t { Buy, Sell };

    std::deque<AggressorSide> samples_;
    size_t buy_count_  = 0;
    size_t sell_count_ = 0;
    size_t last_processed_ = 0;

    void add_sample(AggressorSide side);
};

}  // namespace qf::signals
