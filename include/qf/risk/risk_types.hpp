#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "qf/common/types.hpp"

namespace qf::risk {

// Type of pre-/post-trade risk check.
enum class RiskCheck : uint8_t {
    PositionLimit      = 0,
    NotionalLimit      = 1,
    LossLimit          = 2,
    OrderRateLimit     = 3,
    ConcentrationLimit = 4,
    PnLWarning         = 5,   // Post-trade: PnL approaching loss limit (80%)
    ExposureLimit      = 6    // Post-trade: net or gross exposure breached
};

// Result of a single risk check.
struct RiskResult {
    RiskCheck   check_type;
    bool        passed;
    double      current_value;
    double      limit_value;
    std::string message;
};

// Portfolio-level exposure snapshot.
struct Exposure {
    double net_exposure{0.0};    // sum of signed notional per symbol
    double gross_exposure{0.0};  // sum of absolute notional per symbol

    // Per-symbol net exposure keyed by symbol key.
    std::unordered_map<uint64_t, double> per_symbol;
};

inline const char* to_string(RiskCheck c) {
    switch (c) {
        case RiskCheck::PositionLimit:      return "PositionLimit";
        case RiskCheck::NotionalLimit:      return "NotionalLimit";
        case RiskCheck::LossLimit:          return "LossLimit";
        case RiskCheck::OrderRateLimit:     return "OrderRateLimit";
        case RiskCheck::ConcentrationLimit: return "ConcentrationLimit";
        case RiskCheck::PnLWarning:        return "PnLWarning";
        case RiskCheck::ExposureLimit:     return "ExposureLimit";
    }
    return "Unknown";
}

}  // namespace qf::risk
