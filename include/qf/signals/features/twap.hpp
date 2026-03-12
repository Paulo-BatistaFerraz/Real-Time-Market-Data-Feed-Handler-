#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <deque>
#include <cstdint>

namespace qf::signals {

// Time-Weighted Average Price (TWAP) feature.
// Tracks price over a configurable time window using (price, timestamp) pairs.
// compute() returns time-weighted average of prices within the window.
class TWAP : public FeatureBase {
public:
    // window_ns: rolling window duration in nanoseconds
    explicit TWAP(uint64_t window_ns = 5'000'000'000ULL);  // default 5 seconds

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

private:
    uint64_t window_ns_;

    struct PriceSample {
        double   price;
        uint64_t timestamp;
    };

    std::deque<PriceSample> samples_;

    void evict_old(uint64_t now);
};

}  // namespace qf::signals
