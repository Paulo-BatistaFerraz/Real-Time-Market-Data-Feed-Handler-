#pragma once

#include <cstdint>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// PnLMonitor — post-trade monitor that tracks running PnL.
//
// After every fill, checks total PnL (realized + unrealized) against
// the configured maximum loss.  Fires a warning alert when PnL
// reaches the warning threshold (default 80% of max loss), and
// reports a breach when the limit is fully exceeded.
class PnLMonitor {
public:
    // Construct with max loss (positive number) and warning threshold (0–1).
    // warning_threshold = 0.8 means alert at 80% of max_loss.
    explicit PnLMonitor(double max_loss, double warning_threshold = 0.8);

    // Update internal state from the portfolio after a fill.
    void update(const strategy::Portfolio& portfolio);

    // Check current PnL against limits.
    // Returns a RiskResult with:
    //   - passed = true if PnL is within limits and below warning
    //   - passed = false, check_type = LossLimit if breached
    //   - passed = false, check_type = PnLWarning if approaching limit
    RiskResult check() const;

    // Is PnL at or past the warning threshold (80%)?
    bool is_warning() const;

    // Is the loss limit fully breached?
    bool is_breached() const;

    // Accessors
    double running_pnl() const { return running_pnl_; }
    double max_loss() const { return max_loss_; }
    double warning_threshold() const { return warning_threshold_; }

private:
    double max_loss_;
    double warning_threshold_;
    double running_pnl_{0.0};
};

}  // namespace qf::risk
