#pragma once

#include <vector>
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/strategy/position/portfolio.hpp"
#include "qf/oms/oms_types.hpp"

namespace qf::risk {

// Abstract risk engine interface.
//
// Implementations perform pre-trade checks (before sending an order) and
// post-trade updates (after a fill is received).
class RiskEngine {
public:
    virtual ~RiskEngine() = default;

    // Pre-trade: evaluate a proposed decision against current portfolio state.
    // Returns one RiskResult per configured check.  If any result has
    // passed == false the order should be blocked or reduced.
    virtual std::vector<RiskResult> check_pre_trade(
        const strategy::Decision& decision,
        const strategy::Portfolio& portfolio) = 0;

    // Post-trade: update internal risk monitors after a fill.
    virtual void check_post_trade(
        const oms::FillReport& fill,
        const strategy::Portfolio& portfolio) = 0;

    // Query the current exposure snapshot.
    virtual Exposure current_exposure() const = 0;
};

}  // namespace qf::risk
