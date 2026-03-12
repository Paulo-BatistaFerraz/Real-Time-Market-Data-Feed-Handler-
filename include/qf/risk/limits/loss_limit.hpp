#pragma once

#include <cstdint>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// LossLimit — enforces a maximum daily loss threshold.
//
// Tracks running PnL (realized + unrealized) and rejects new orders
// when the PnL breaches -max_daily_loss.  The running PnL can be
// reset at a configurable interval (e.g. start of each trading day).
class LossLimit {
public:
    // Construct with a maximum daily loss (positive number, e.g. 10000.0 = $10k).
    // |reset_interval_ns| is the interval in nanoseconds at which PnL resets
    // (default 0 = manual reset only).
    explicit LossLimit(double max_daily_loss,
                       uint64_t reset_interval_ns = 0);

    // Pre-trade check: is the running PnL above the loss floor?
    // |current_ts| is used to detect interval boundaries for auto-reset.
    RiskResult check(const strategy::Decision& decision,
                     const strategy::Portfolio& portfolio,
                     Timestamp current_ts = 0) const;

    // Record a PnL update (delta, e.g. from a fill).
    void record_pnl(double pnl_delta);

    // Set running PnL directly (e.g. from portfolio snapshot).
    void set_running_pnl(double pnl);

    // Manual reset of running PnL.
    void reset();

    // Accessors.
    double running_pnl() const { return running_pnl_; }
    double max_daily_loss() const { return max_daily_loss_; }

private:
    double   max_daily_loss_;
    uint64_t reset_interval_ns_;

    mutable double   running_pnl_{0.0};
    mutable uint64_t last_reset_ts_{0};

    void maybe_reset(Timestamp current_ts) const;
};

}  // namespace qf::risk
