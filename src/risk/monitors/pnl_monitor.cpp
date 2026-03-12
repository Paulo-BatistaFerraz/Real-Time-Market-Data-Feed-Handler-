#include "qf/risk/monitors/pnl_monitor.hpp"
#include "qf/common/logging.hpp"

#include <cmath>
#include <sstream>

namespace qf::risk {

PnLMonitor::PnLMonitor(double max_loss, double warning_threshold)
    : max_loss_(max_loss)
    , warning_threshold_(warning_threshold) {}

void PnLMonitor::update(const strategy::Portfolio& portfolio) {
    auto snap = portfolio.snapshot();
    running_pnl_ = snap.total_pnl;
}

RiskResult PnLMonitor::check() const {
    // Loss limit is breached when PnL drops below -max_loss_.
    if (is_breached()) {
        std::ostringstream msg;
        msg << "PnL " << running_pnl_ << " breaches loss limit " << -max_loss_;
        return RiskResult{
            RiskCheck::LossLimit,
            false,
            running_pnl_,
            -max_loss_,
            msg.str()
        };
    }

    // Warning fires when PnL exceeds warning_threshold_ of max_loss_.
    if (is_warning()) {
        std::ostringstream msg;
        msg << "PnL " << running_pnl_
            << " approaching loss limit (warning at "
            << (warning_threshold_ * 100.0) << "%)";
        return RiskResult{
            RiskCheck::PnLWarning,
            false,
            running_pnl_,
            -(max_loss_ * warning_threshold_),
            msg.str()
        };
    }

    return RiskResult{
        RiskCheck::LossLimit,
        true,
        running_pnl_,
        -max_loss_,
        ""
    };
}

bool PnLMonitor::is_warning() const {
    // Warning when loss reaches warning_threshold_ fraction of max_loss_.
    // e.g. if max_loss_ = 50000 and threshold = 0.8, warn at PnL <= -40000.
    return running_pnl_ <= -(max_loss_ * warning_threshold_);
}

bool PnLMonitor::is_breached() const {
    return running_pnl_ <= -max_loss_;
}

}  // namespace qf::risk
