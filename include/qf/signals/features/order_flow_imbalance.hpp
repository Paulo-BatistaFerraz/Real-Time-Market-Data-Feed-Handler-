#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <deque>
#include <cstdint>

namespace qf::signals {

// Order Flow Imbalance (OFI) feature.
// Tracks net buy vs sell volume over a rolling time window.
// compute() returns (buy_volume - sell_volume) / (buy_volume + sell_volume),
// clamped to [-1, +1].  +1 = all buying, -1 = all selling, 0 = balanced.
class OrderFlowImbalance : public FeatureBase {
public:
    // window_ns: rolling window duration in nanoseconds (default 5 seconds)
    explicit OrderFlowImbalance(uint64_t window_ns = 5'000'000'000ULL);

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

private:
    uint64_t window_ns_;

    struct VolumeSample {
        double   quantity;
        Side     side;
        uint64_t timestamp;
    };

    std::deque<VolumeSample> samples_;
    size_t last_processed_ = 0;

    void evict_old(uint64_t now);
};

}  // namespace qf::signals
