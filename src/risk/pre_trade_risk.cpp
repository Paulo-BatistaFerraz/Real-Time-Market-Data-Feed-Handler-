#include "qf/risk/pre_trade_risk.hpp"
#include "qf/common/logging.hpp"

namespace qf::risk {

PreTradeRisk::PreTradeRisk(const PreTradeRiskConfig& config)
    : fail_fast_(config.fail_fast)
    , position_limit_(config.max_position_shares)
    , notional_limit_(config.max_notional_dollars)
    , loss_limit_(config.max_daily_loss)
    , order_rate_limit_(config.max_orders_per_second)
    , concentration_limit_(config.max_concentration_pct) {}

std::vector<RiskResult> PreTradeRisk::check_pre_trade(
        const strategy::Decision& decision,
        const strategy::Portfolio& portfolio) {

    std::vector<RiskResult> results;
    results.reserve(5);

    const Price ref_price = reference_price(decision);
    bool all_passed = true;

    // Helper: run one check, log result, optionally short-circuit.
    auto run_check = [&](auto&& check_fn, const char* name) -> bool {
        RiskResult result = check_fn();
        if (result.passed) {
            QF_LOG_DEBUG_T("RISK", "{} PASSED (value={:.2f}, limit={:.2f})",
                           name, result.current_value, result.limit_value);
        } else {
            QF_LOG_WARN_T("RISK", "{} FAILED: {} (value={:.2f}, limit={:.2f})",
                          name, result.message, result.current_value, result.limit_value);
            all_passed = false;
        }
        results.push_back(std::move(result));
        return result.passed;
    };

    // 1. Position limit
    bool ok = run_check([&]() {
        return position_limit_.check(decision, portfolio);
    }, "PositionLimit");
    if (!ok && fail_fast_) return results;

    // 2. Notional limit
    ok = run_check([&]() {
        return notional_limit_.check(decision, portfolio, ref_price);
    }, "NotionalLimit");
    if (!ok && fail_fast_) return results;

    // 3. Loss limit
    ok = run_check([&]() {
        return loss_limit_.check(decision, portfolio, current_ts_);
    }, "LossLimit");
    if (!ok && fail_fast_) return results;

    // 4. Order rate limit
    ok = run_check([&]() {
        return order_rate_limit_.check(current_ts_);
    }, "OrderRateLimit");
    if (!ok && fail_fast_) return results;

    // 5. Concentration limit
    ok = run_check([&]() {
        return concentration_limit_.check(decision, portfolio, prices_, ref_price);
    }, "ConcentrationLimit");
    // No need to check fail_fast on last item.

    if (all_passed) {
        QF_LOG_INFO_T("RISK", "PreTradeRisk: all checks PASSED");
    } else {
        // Count failures for summary log.
        size_t fail_count = 0;
        for (const auto& r : results) {
            if (!r.passed) ++fail_count;
        }
        QF_LOG_WARN_T("RISK", "PreTradeRisk: REJECTED ({} of {} checks failed)",
                       fail_count, results.size());
    }

    return results;
}

void PreTradeRisk::check_post_trade(
        const oms::FillReport& fill,
        const strategy::Portfolio& portfolio) {
    // Update the loss limit with new PnL from portfolio.
    auto snap = portfolio.snapshot();
    loss_limit_.set_running_pnl(snap.total_pnl);

    // Record the order timestamp for rate limiting.
    order_rate_limit_.record_order(fill.timestamp);
}

Exposure PreTradeRisk::current_exposure() const {
    // Build a default empty exposure; callers can use
    // notional_limit().compute_exposure() with live prices.
    return Exposure{};
}

void PreTradeRisk::set_fail_fast(bool enabled) {
    fail_fast_ = enabled;
}

void PreTradeRisk::set_price(const Symbol& symbol, Price price) {
    prices_[symbol.as_key()] = price;
}

void PreTradeRisk::set_current_timestamp(Timestamp ts) {
    current_ts_ = ts;
}

Price PreTradeRisk::reference_price(const strategy::Decision& decision) const {
    // Use limit price for limit orders, otherwise fall back to stored price.
    if (decision.order_type == strategy::OrderType::Limit &&
        decision.limit_price > 0) {
        return decision.limit_price;
    }
    auto it = prices_.find(decision.symbol.as_key());
    return (it != prices_.end()) ? it->second : 0;
}

}  // namespace qf::risk
