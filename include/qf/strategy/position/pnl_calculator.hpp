#pragma once

#include <cstdint>
#include <deque>
#include <unordered_map>
#include "qf/common/types.hpp"
#include "qf/strategy/position/fill_tracker.hpp"

namespace qf::strategy {

// PnLCalculator — computes realized and unrealized PnL.
//
// Realized PnL is computed using FIFO accounting: when a fill closes
// (or partially closes) an existing position, the PnL equals
// (exit_price - entry_price) * quantity for longs, inverted for shorts.
//
// Unrealized PnL is computed by marking open lots to the current
// market price via mark_to_market().
//
// Thread-safety: not thread-safe; callers must synchronise externally.
class PnLCalculator {
public:
    // Process a fill — updates realized PnL using FIFO when the fill
    // reduces an existing position.
    void on_fill(const Fill& fill);

    // Mark open lots for |symbol| to |current_price| and update
    // unrealized PnL.
    void mark_to_market(const Symbol& symbol, Price current_price);

    // Realized PnL for a single symbol (fixed-point / 10000).
    double realized_pnl(const Symbol& symbol) const;

    // Unrealized PnL for a single symbol.
    double unrealized_pnl(const Symbol& symbol) const;

    // Total realized PnL across all symbols.
    double total_realized_pnl() const;

    // Total unrealized PnL across all symbols.
    double total_unrealized_pnl() const;

    // Total PnL = realized + unrealized.
    double total_pnl() const;

    // Reset all state.
    void reset();

private:
    // A single lot in the FIFO queue.
    struct Lot {
        Price    price;
        uint32_t quantity;
    };

    // Per-symbol FIFO state.
    struct SymbolState {
        std::deque<Lot> long_lots;   // open long lots (bought)
        std::deque<Lot> short_lots;  // open short lots (sold short)
        double realized{0.0};
        double unrealized{0.0};
    };

    SymbolState& state(const Symbol& symbol);
    const SymbolState* find_state(const Symbol& symbol) const;

    std::unordered_map<uint64_t, SymbolState> states_;
};

}  // namespace qf::strategy
