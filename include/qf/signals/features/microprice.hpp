#pragma once

#include "qf/signals/features/feature_base.hpp"

namespace qf::signals {

// Size-weighted microprice feature.
// Computes: (bid_price * ask_qty + ask_price * bid_qty) / (bid_qty + ask_qty)
// Returns NaN if no bid or ask is available.
class Microprice : public FeatureBase {
public:
    Microprice() = default;

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;
};

}  // namespace qf::signals
