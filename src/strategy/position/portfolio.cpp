#include "qf/strategy/position/portfolio.hpp"

namespace qf::strategy {

void Portfolio::on_fill(Timestamp ts, const Symbol& symbol, Side side,
                        Price price, Quantity quantity) {
    // Track this symbol for snapshot iteration.
    symbols_[symbol.as_key()] = symbol;

    // Update all three components.
    positions_.update(symbol, side, quantity);
    fills_.record(ts, symbol, side, price, quantity);

    Fill fill{ts, symbol, side, price, quantity};
    pnl_.on_fill(fill);
}

void Portfolio::mark_to_market(const Symbol& symbol, Price current_price) {
    pnl_.mark_to_market(symbol, current_price);
}

PortfolioSnapshot Portfolio::snapshot() const {
    PortfolioSnapshot snap;

    // Positions.
    snap.positions = positions_.get_all();

    // Fills and per-symbol PnL.
    for (const auto& [key, sym] : symbols_) {
        const auto& sym_fills = fills_.fills(sym);
        if (!sym_fills.empty()) {
            snap.fills[key] = sym_fills;
        }
        snap.realized_pnl[key]   = pnl_.realized_pnl(sym);
        snap.unrealized_pnl[key] = pnl_.unrealized_pnl(sym);
    }

    // Totals.
    snap.total_realized_pnl   = pnl_.total_realized_pnl();
    snap.total_unrealized_pnl = pnl_.total_unrealized_pnl();
    snap.total_pnl            = pnl_.total_pnl();
    snap.total_fills          = fills_.total_fills();

    return snap;
}

void Portfolio::reset() {
    positions_.reset();
    fills_.reset();
    pnl_.reset();
    symbols_.clear();
}

}  // namespace qf::strategy
