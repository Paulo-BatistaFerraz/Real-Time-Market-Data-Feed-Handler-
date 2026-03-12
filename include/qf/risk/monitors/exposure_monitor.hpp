#pragma once

#include <cstdint>
#include <unordered_map>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// ExposureMonitor — post-trade monitor for net and gross exposure.
//
// Tracks per-symbol and total net/gross notional exposure.
// After every fill, recomputes exposure from portfolio positions
// and reference prices, then checks against configured limits.
class ExposureMonitor {
public:
    // Construct with global net and gross exposure limits (dollars).
    explicit ExposureMonitor(double max_net_exposure, double max_gross_exposure);

    // Set a reference price for notional calculations.
    void set_price(const Symbol& symbol, Price price);

    // Update internal exposure state from portfolio after a fill.
    void update(const strategy::Portfolio& portfolio);

    // Check current exposure against limits.
    // Returns a RiskResult with passed = false if net or gross is breached.
    RiskResult check() const;

    // Current exposure snapshot (net, gross, per-symbol).
    Exposure current_exposure() const { return exposure_; }

    // Per-symbol net exposure in dollars.
    double symbol_net_exposure(const Symbol& symbol) const;

    // Accessors
    double max_net_exposure() const { return max_net_; }
    double max_gross_exposure() const { return max_gross_; }

private:
    double max_net_;
    double max_gross_;

    // Reference prices: symbol_key -> fixed-point price.
    std::unordered_map<uint64_t, Price> prices_;

    // Cached exposure state (recomputed on update).
    Exposure exposure_;
};

}  // namespace qf::risk
