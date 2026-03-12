#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/risk/monitors/var_calculator.hpp"
#include "qf/risk/monitors/circuit_breaker.hpp"
#include "qf/risk/pre_trade_risk.hpp"
#include "qf/risk/post_trade_risk.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// A complete risk snapshot capturing all positions, exposures, PnL,
// limit utilizations, VaR, and circuit breaker state at a point in time.
struct RiskSnapshot {
    // Positions: symbol_key -> signed quantity
    std::unordered_map<uint64_t, int64_t> positions;

    // Exposure
    Exposure exposure;

    // PnL
    double total_realized_pnl{0.0};
    double total_unrealized_pnl{0.0};
    double total_pnl{0.0};

    // Limit utilizations (fraction 0–1, where 1.0 = fully utilized)
    struct LimitUtilization {
        RiskCheck check_type;
        double    utilization;   // current / limit (0–1+)
        double    current_value;
        double    limit_value;
    };
    std::vector<LimitUtilization> limit_utilizations;

    // VaR
    PortfolioVaRResult var;

    // Circuit breaker state
    bool   circuit_breaker_tripped{false};
    double circuit_breaker_drawdown{0.0};
    double circuit_breaker_threshold{0.0};

    // Snapshot timestamp
    Timestamp timestamp{0};
};

// RiskReport — generates a consolidated risk snapshot from all risk components.
class RiskReport {
public:
    // Construct with references to the risk components.
    RiskReport(PreTradeRisk& pre_trade,
               PostTradeRisk& post_trade,
               VaRCalculator& var_calc,
               CircuitBreaker& breaker);

    // Generate a complete risk snapshot.
    RiskSnapshot snapshot(const strategy::Portfolio& portfolio,
                          Timestamp ts = 0) const;

private:
    PreTradeRisk&   pre_trade_;
    PostTradeRisk&  post_trade_;
    VaRCalculator&  var_calc_;
    CircuitBreaker& breaker_;
};

}  // namespace qf::risk
