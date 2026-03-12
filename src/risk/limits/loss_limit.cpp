#include "qf/risk/limits/loss_limit.hpp"

#include <cmath>
#include <sstream>

namespace qf::risk {

LossLimit::LossLimit(double max_daily_loss, uint64_t reset_interval_ns)
    : max_daily_loss_(max_daily_loss)
    , reset_interval_ns_(reset_interval_ns) {}

RiskResult LossLimit::check(const strategy::Decision& /*decision*/,
                             const strategy::Portfolio& portfolio,
                             Timestamp current_ts) const {
    // Auto-reset if interval is configured and boundary crossed.
    maybe_reset(current_ts);

    // Use portfolio's total PnL as the authoritative running value when
    // available, but also incorporate any externally recorded deltas.
    const double portfolio_pnl = portfolio.pnl().total_pnl();
    const double effective_pnl = running_pnl_ + portfolio_pnl;

    // Loss floor: PnL must be > -max_daily_loss (i.e. losses capped).
    const double loss_floor = -max_daily_loss_;
    const bool passed = effective_pnl > loss_floor;

    std::ostringstream msg;
    if (!passed) {
        msg << "Daily loss limit breached: PnL $" << effective_pnl
            << " <= -$" << max_daily_loss_;
    }

    return RiskResult{
        RiskCheck::LossLimit,
        passed,
        effective_pnl,
        loss_floor,
        msg.str()
    };
}

void LossLimit::record_pnl(double pnl_delta) {
    running_pnl_ += pnl_delta;
}

void LossLimit::set_running_pnl(double pnl) {
    running_pnl_ = pnl;
}

void LossLimit::reset() {
    running_pnl_ = 0.0;
}

void LossLimit::maybe_reset(Timestamp current_ts) const {
    if (reset_interval_ns_ == 0 || current_ts == 0) return;

    if (last_reset_ts_ == 0) {
        last_reset_ts_ = current_ts;
        return;
    }

    if (current_ts - last_reset_ts_ >= reset_interval_ns_) {
        running_pnl_ = 0.0;
        last_reset_ts_ = current_ts;
    }
}

}  // namespace qf::risk
