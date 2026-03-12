#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/strategy/position/position_tracker.hpp"
#include "qf/strategy/position/fill_tracker.hpp"
#include "qf/strategy/position/pnl_calculator.hpp"

namespace qf::strategy {

// Complete portfolio state snapshot.
struct PortfolioSnapshot {
    // Net position per symbol (keyed by symbol key).
    std::unordered_map<uint64_t, int64_t> positions;

    // All fills per symbol.
    std::unordered_map<uint64_t, std::vector<Fill>> fills;

    // Realized PnL per symbol.
    std::unordered_map<uint64_t, double> realized_pnl;

    // Unrealized PnL per symbol.
    std::unordered_map<uint64_t, double> unrealized_pnl;

    // Aggregate totals.
    double total_realized_pnl{0.0};
    double total_unrealized_pnl{0.0};
    double total_pnl{0.0};

    size_t total_fills{0};
};

// Portfolio — aggregates PositionTracker + FillTracker + PnLCalculator.
//
// Provides a unified interface for recording fills and querying the
// complete state of positions, fills, and PnL.
//
// Thread-safety: not thread-safe; callers must synchronise externally.
class Portfolio {
public:
    // Record a fill — updates positions, fills, and PnL.
    void on_fill(Timestamp ts, const Symbol& symbol, Side side,
                 Price price, Quantity quantity);

    // Mark a symbol to current market price for unrealized PnL.
    void mark_to_market(const Symbol& symbol, Price current_price);

    // Return a complete state snapshot.
    PortfolioSnapshot snapshot() const;

    // Access individual components.
    const PositionTracker& positions() const { return positions_; }
    const FillTracker& fills() const { return fills_; }
    const PnLCalculator& pnl() const { return pnl_; }

    // Reset all state.
    void reset();

private:
    PositionTracker positions_;
    FillTracker     fills_;
    PnLCalculator   pnl_;

    // Track which symbols we've seen (for snapshot iteration).
    std::unordered_map<uint64_t, Symbol> symbols_;
};

}  // namespace qf::strategy
