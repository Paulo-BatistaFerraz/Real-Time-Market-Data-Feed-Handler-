#include "qf/risk/reporting/risk_logger.hpp"
#include "qf/common/logging.hpp"

namespace qf::risk {

void RiskLogger::log_pre_trade(const strategy::Decision& decision,
                               const std::vector<RiskResult>& results) {
    const char* side_str = (decision.side == Side::Buy) ? "BUY" : "SELL";

    QF_LOG_INFO_T("RISK_AUDIT", "PRE-TRADE check: {} {} qty={}",
                  side_str, decision.symbol.data, decision.quantity);

    size_t fail_count = 0;
    for (const auto& r : results) {
        if (r.passed) {
            QF_LOG_DEBUG_T("RISK_AUDIT", "  {} PASSED (current={:.2f}, limit={:.2f})",
                           to_string(r.check_type), r.current_value, r.limit_value);
        } else {
            QF_LOG_WARN_T("RISK_AUDIT", "  {} FAILED: {} (current={:.2f}, limit={:.2f})",
                          to_string(r.check_type), r.message,
                          r.current_value, r.limit_value);
            ++fail_count;
        }
    }

    if (fail_count == 0) {
        QF_LOG_INFO_T("RISK_AUDIT", "PRE-TRADE result: APPROVED ({} checks passed)",
                      results.size());
    } else {
        QF_LOG_WARN_T("RISK_AUDIT", "PRE-TRADE result: REJECTED ({} of {} failed)",
                      fail_count, results.size());
    }
}

void RiskLogger::log_post_trade(const oms::FillReport& fill,
                                const std::vector<RiskResult>& breaches) {
    QF_LOG_INFO_T("RISK_AUDIT",
                  "POST-TRADE update: order={} fill_qty={} fill_price={:.4f} final={}",
                  fill.order_id, fill.fill_quantity,
                  price_to_double(fill.fill_price),
                  fill.is_final ? "true" : "false");

    if (breaches.empty()) {
        QF_LOG_DEBUG_T("RISK_AUDIT", "  POST-TRADE: all monitors OK");
    } else {
        for (const auto& r : breaches) {
            QF_LOG_WARN_T("RISK_AUDIT", "  POST-TRADE {}: {} (current={:.2f}, limit={:.2f})",
                          to_string(r.check_type), r.message,
                          r.current_value, r.limit_value);
        }
    }
}

void RiskLogger::log_breaker_event(const CircuitBreakerTripped& event) {
    QF_LOG_ERROR_T("RISK_AUDIT",
                   "CIRCUIT BREAKER TRIPPED: reason=\"{}\" drawdown={:.4f} limit={:.4f} manual={}",
                   event.reason, event.drawdown, event.drawdown_limit,
                   event.manual ? "true" : "false");
}

void RiskLogger::log_breaker_reset() {
    QF_LOG_INFO_T("RISK_AUDIT", "CIRCUIT BREAKER RESET: trading re-enabled");
}

}  // namespace qf::risk
