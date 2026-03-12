#pragma once

#include <string>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/risk/monitors/circuit_breaker.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/oms/oms_types.hpp"

namespace qf::risk {

// RiskLogger — structured audit trail for every risk decision.
//
// Logs every pre-trade check (passed/failed), every post-trade update,
// and every circuit breaker event to the structured logging subsystem.
class RiskLogger {
public:
    // Log the results of a pre-trade check for a given decision.
    void log_pre_trade(const strategy::Decision& decision,
                       const std::vector<RiskResult>& results);

    // Log a post-trade update after a fill.
    void log_post_trade(const oms::FillReport& fill,
                        const std::vector<RiskResult>& breaches);

    // Log a circuit breaker event (trip or reset).
    void log_breaker_event(const CircuitBreakerTripped& event);

    // Log a circuit breaker reset.
    void log_breaker_reset();
};

}  // namespace qf::risk
