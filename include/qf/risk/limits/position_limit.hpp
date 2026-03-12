#pragma once

#include <cstdint>
#include <unordered_map>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// PositionLimit — enforces maximum position size per symbol and globally.
//
// Checks that current_position + order_quantity <= max_shares for the
// relevant symbol (and global aggregate).  Configurable per-symbol overrides
// allow tighter or looser limits for individual instruments.
class PositionLimit {
public:
    // Construct with a global max position (shares).  Per-symbol overrides
    // can be added afterwards.
    explicit PositionLimit(int64_t global_max_shares);

    // Set a per-symbol override.
    void set_symbol_limit(const Symbol& symbol, int64_t max_shares);

    // Pre-trade check: would the proposed decision breach limits?
    RiskResult check(const strategy::Decision& decision,
                     const strategy::Portfolio& portfolio) const;

    // Accessor for the global max position limit.
    int64_t global_max() const { return global_max_; }

private:
    int64_t global_max_;
    std::unordered_map<uint64_t, int64_t> symbol_limits_;

    int64_t limit_for(uint64_t symbol_key) const;
};

}  // namespace qf::risk
