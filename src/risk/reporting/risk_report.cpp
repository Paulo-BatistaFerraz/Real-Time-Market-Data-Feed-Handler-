#include "qf/risk/reporting/risk_report.hpp"
#include "qf/common/logging.hpp"

#include <cmath>

namespace qf::risk {

RiskReport::RiskReport(PreTradeRisk& pre_trade,
                       PostTradeRisk& post_trade,
                       VaRCalculator& var_calc,
                       CircuitBreaker& breaker)
    : pre_trade_(pre_trade)
    , post_trade_(post_trade)
    , var_calc_(var_calc)
    , breaker_(breaker) {}

RiskSnapshot RiskReport::snapshot(const strategy::Portfolio& portfolio,
                                  Timestamp ts) const {
    RiskSnapshot snap;
    snap.timestamp = ts;

    // 1. Positions
    auto port_snap = portfolio.snapshot();
    snap.positions = port_snap.positions;

    // 2. PnL
    snap.total_realized_pnl = port_snap.total_realized_pnl;
    snap.total_unrealized_pnl = port_snap.total_unrealized_pnl;
    snap.total_pnl = port_snap.total_pnl;

    // 3. Exposure from post-trade monitors
    snap.exposure = post_trade_.exposure_monitor().current_exposure();

    // 4. Limit utilizations
    //    Position limit: use largest per-symbol utilization
    {
        double max_util = 0.0;
        int64_t max_abs = 0;
        for (const auto& [sym_key, qty] : snap.positions) {
            int64_t abs_qty = std::abs(qty);
            if (abs_qty > max_abs) max_abs = abs_qty;
        }
        double limit_val = static_cast<double>(pre_trade_.position_limit().global_max());
        if (limit_val > 0.0) {
            max_util = static_cast<double>(max_abs) / limit_val;
        }
        snap.limit_utilizations.push_back({
            RiskCheck::PositionLimit,
            max_util,
            static_cast<double>(max_abs),
            limit_val
        });
    }

    //    Loss limit utilization
    {
        double max_loss = pre_trade_.loss_limit().max_daily_loss();
        double running = pre_trade_.loss_limit().running_pnl() + port_snap.total_pnl;
        double util = (max_loss > 0.0) ? std::abs(std::min(0.0, running)) / max_loss : 0.0;
        snap.limit_utilizations.push_back({
            RiskCheck::LossLimit,
            util,
            running,
            -max_loss
        });
    }

    //    PnL monitor utilization (post-trade)
    {
        double max_loss = post_trade_.pnl_monitor().max_loss();
        double pnl = post_trade_.pnl_monitor().running_pnl();
        double util = (max_loss > 0.0) ? std::abs(std::min(0.0, pnl)) / max_loss : 0.0;
        snap.limit_utilizations.push_back({
            RiskCheck::PnLWarning,
            util,
            pnl,
            -max_loss
        });
    }

    //    Exposure utilization
    {
        double gross = snap.exposure.gross_exposure;
        double max_gross = post_trade_.exposure_monitor().max_gross_exposure();
        double util = (max_gross > 0.0) ? gross / max_gross : 0.0;
        snap.limit_utilizations.push_back({
            RiskCheck::ExposureLimit,
            util,
            gross,
            max_gross
        });
    }

    // 5. VaR
    snap.var = var_calc_.compute(portfolio);

    // 6. Circuit breaker state
    snap.circuit_breaker_tripped = breaker_.is_tripped();
    snap.circuit_breaker_drawdown = breaker_.current_drawdown();
    snap.circuit_breaker_threshold = breaker_.drawdown_threshold();

    QF_LOG_INFO_T("RISK", "RiskReport snapshot: pnl={:.2f}, var={:.2f}, cb_tripped={}",
                  snap.total_pnl, snap.var.portfolio_var,
                  snap.circuit_breaker_tripped ? "true" : "false");

    return snap;
}

}  // namespace qf::risk
