#include "qf/risk/post_trade_risk.hpp"
#include "qf/common/logging.hpp"

namespace qf::risk {

PostTradeRisk::PostTradeRisk(const PostTradeRiskConfig& config)
    : pnl_monitor_(config.max_daily_loss, config.pnl_warning_threshold)
    , exposure_monitor_(config.max_net_exposure, config.max_gross_exposure) {}

std::vector<RiskResult> PostTradeRisk::on_fill(
        const oms::FillReport& fill,
        const strategy::Portfolio& portfolio) {

    // Update all monitors with current portfolio state (already reflects fill).
    pnl_monitor_.update(portfolio);
    exposure_monitor_.update(portfolio);

    // Collect breached limits.
    std::vector<RiskResult> breaches;

    // Check PnL monitor.
    RiskResult pnl_result = pnl_monitor_.check();
    if (!pnl_result.passed) {
        if (pnl_monitor_.is_breached()) {
            QF_LOG_ERROR_T("RISK", "PostTrade PnL BREACHED: {} (pnl={:.2f}, limit={:.2f})",
                           pnl_result.message, pnl_result.current_value, pnl_result.limit_value);
        } else {
            QF_LOG_WARN_T("RISK", "PostTrade PnL WARNING: {} (pnl={:.2f}, threshold={:.2f})",
                          pnl_result.message, pnl_result.current_value, pnl_result.limit_value);
        }
        breaches.push_back(std::move(pnl_result));
    } else {
        QF_LOG_DEBUG_T("RISK", "PostTrade PnL OK (pnl={:.2f})", pnl_result.current_value);
    }

    // Check exposure monitor.
    RiskResult exp_result = exposure_monitor_.check();
    if (!exp_result.passed) {
        QF_LOG_ERROR_T("RISK", "PostTrade Exposure BREACHED: {} (value={:.2f}, limit={:.2f})",
                       exp_result.message, exp_result.current_value, exp_result.limit_value);
        breaches.push_back(std::move(exp_result));
    } else {
        QF_LOG_DEBUG_T("RISK", "PostTrade Exposure OK (gross={:.2f})", exp_result.current_value);
    }

    if (breaches.empty()) {
        QF_LOG_DEBUG_T("RISK", "PostTradeRisk: all monitors OK after fill on order {}",
                       fill.order_id);
    } else {
        QF_LOG_WARN_T("RISK", "PostTradeRisk: {} breach(es) after fill on order {}",
                       breaches.size(), fill.order_id);
    }

    return breaches;
}

void PostTradeRisk::set_price(const Symbol& symbol, Price price) {
    exposure_monitor_.set_price(symbol, price);
}

}  // namespace qf::risk
