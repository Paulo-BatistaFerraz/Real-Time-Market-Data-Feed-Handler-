#pragma once

#include <string>
#include "qf/core/order_book.hpp"
#include "qf/signals/signal_types.hpp"

namespace qf::signals {

// Abstract base class for all features computed from order book and trade data.
//
// Features are the raw numeric inputs to the signal engine — each one extracts
// a single dimension (e.g. VWAP, microprice, order flow imbalance) from the
// current book state and recent trade history.
//
// Implementations must be stateless with respect to compute(): given the same
// OrderBook and TradeHistory, they must return the same value.
class FeatureBase {
public:
    virtual ~FeatureBase() = default;

    // Compute the feature value from current book state and trade history.
    virtual double compute(const core::OrderBook& book, const TradeHistory& trades) = 0;

    // Unique name identifying this feature (e.g. "vwap", "microprice", "ofi").
    virtual std::string name() = 0;
};

}  // namespace qf::signals
