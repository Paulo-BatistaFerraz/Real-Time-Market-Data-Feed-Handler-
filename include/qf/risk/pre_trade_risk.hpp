#pragma once

#include <unordered_map>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/risk/risk_engine.hpp"
#include "qf/risk/limits/position_limit.hpp"
#include "qf/risk/limits/notional_limit.hpp"
#include "qf/risk/limits/loss_limit.hpp"
#include "qf/risk/limits/order_rate_limit.hpp"
#include "qf/risk/limits/concentration_limit.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// Configuration for the PreTradeRisk orchestrator.
struct PreTradeRiskConfig {
    // Position limit: max shares per symbol (global default).
    int64_t max_position_shares{10000};

    // Notional limit: max dollar exposure (global default).
    double max_notional_dollars{1'000'000.0};

    // Loss limit: max daily loss before blocking orders.
    double max_daily_loss{50'000.0};

    // Order rate limit: max orders per second.
    uint32_t max_orders_per_second{100};

    // Concentration limit: max single-symbol percentage (0–1).
    double max_concentration_pct{0.25};

    // If true, stop at the first failing check (fail-fast).
    // If false, run all checks and collect every failure.
    bool fail_fast{false};
};

// PreTradeRisk — orchestrates all pre-trade risk checks.
//
// Implements the RiskEngine interface.  On each call to check_pre_trade(),
// runs the five limit checks in order:
//   1. Position  2. Notional  3. Loss  4. Rate  5. Concentration
//
// In fail-fast mode, returns immediately after the first failed check.
// In check-all mode, runs every check and returns all results.
// Logs every individual check result.
class PreTradeRisk : public RiskEngine {
public:
    explicit PreTradeRisk(const PreTradeRiskConfig& config);

    // RiskEngine interface ------------------------------------------------

    // Run all pre-trade checks.  Returns Approved (all passed) or
    // Rejected(reasons) with a list of every failed check.
    std::vector<RiskResult> check_pre_trade(
        const strategy::Decision& decision,
        const strategy::Portfolio& portfolio) override;

    // Post-trade: update loss tracker and order-rate window.
    void check_post_trade(
        const oms::FillReport& fill,
        const strategy::Portfolio& portfolio) override;

    // Current exposure snapshot (delegates to NotionalLimit).
    Exposure current_exposure() const override;

    // Configuration -------------------------------------------------------

    // Set fail-fast mode at runtime.
    void set_fail_fast(bool enabled);
    bool fail_fast() const { return fail_fast_; }

    // Set reference prices for notional/concentration calculations.
    void set_price(const Symbol& symbol, Price price);

    // Set a current timestamp for rate-limit and loss-limit checks.
    void set_current_timestamp(Timestamp ts);

    // Per-symbol overrides on individual limits.
    PositionLimit&      position_limit()      { return position_limit_; }
    NotionalLimit&      notional_limit()      { return notional_limit_; }
    LossLimit&          loss_limit()          { return loss_limit_; }
    OrderRateLimit&     order_rate_limit()    { return order_rate_limit_; }
    ConcentrationLimit& concentration_limit() { return concentration_limit_; }

private:
    bool fail_fast_;
    Timestamp current_ts_{0};

    // Reference prices for notional/concentration calcs.
    std::unordered_map<uint64_t, Price> prices_;

    // Individual limit checkers (owned).
    PositionLimit      position_limit_;
    NotionalLimit      notional_limit_;
    LossLimit          loss_limit_;
    OrderRateLimit     order_rate_limit_;
    ConcentrationLimit concentration_limit_;

    // Resolve the reference price for a decision.
    Price reference_price(const strategy::Decision& decision) const;
};

}  // namespace qf::risk
