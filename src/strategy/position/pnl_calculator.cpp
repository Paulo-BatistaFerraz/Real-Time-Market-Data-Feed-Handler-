#include "qf/strategy/position/pnl_calculator.hpp"

namespace qf::strategy {

void PnLCalculator::on_fill(const Fill& fill) {
    auto& s = state(fill.symbol);
    const auto qty = fill.quantity;
    const auto px  = fill.price;

    if (fill.side == Side::Buy) {
        // A buy closes short lots (FIFO), remainder opens new long lots.
        uint32_t remaining = qty;

        while (remaining > 0 && !s.short_lots.empty()) {
            auto& front = s.short_lots.front();
            const uint32_t matched = std::min(remaining, front.quantity);

            // Short sold at front.price, buying back at px.
            // PnL = (entry - exit) per unit for shorts.
            const double pnl_per_unit =
                price_to_double(front.price) - price_to_double(px);
            s.realized += pnl_per_unit * matched;

            front.quantity -= matched;
            remaining -= matched;

            if (front.quantity == 0) {
                s.short_lots.pop_front();
            }
        }

        if (remaining > 0) {
            s.long_lots.push_back(Lot{px, remaining});
        }
    } else {
        // A sell closes long lots (FIFO), remainder opens new short lots.
        uint32_t remaining = qty;

        while (remaining > 0 && !s.long_lots.empty()) {
            auto& front = s.long_lots.front();
            const uint32_t matched = std::min(remaining, front.quantity);

            // Bought at front.price, selling at px.
            // PnL = (exit - entry) per unit for longs.
            const double pnl_per_unit =
                price_to_double(px) - price_to_double(front.price);
            s.realized += pnl_per_unit * matched;

            front.quantity -= matched;
            remaining -= matched;

            if (front.quantity == 0) {
                s.long_lots.pop_front();
            }
        }

        if (remaining > 0) {
            s.short_lots.push_back(Lot{px, remaining});
        }
    }
}

void PnLCalculator::mark_to_market(const Symbol& symbol, Price current_price) {
    auto& s = state(symbol);
    double unrealized = 0.0;
    const double mark = price_to_double(current_price);

    // Unrealized PnL on open long lots.
    for (const auto& lot : s.long_lots) {
        unrealized += (mark - price_to_double(lot.price)) * lot.quantity;
    }

    // Unrealized PnL on open short lots.
    for (const auto& lot : s.short_lots) {
        unrealized += (price_to_double(lot.price) - mark) * lot.quantity;
    }

    s.unrealized = unrealized;
}

double PnLCalculator::realized_pnl(const Symbol& symbol) const {
    const auto* s = find_state(symbol);
    return s ? s->realized : 0.0;
}

double PnLCalculator::unrealized_pnl(const Symbol& symbol) const {
    const auto* s = find_state(symbol);
    return s ? s->unrealized : 0.0;
}

double PnLCalculator::total_realized_pnl() const {
    double total = 0.0;
    for (const auto& [key, s] : states_) {
        total += s.realized;
    }
    return total;
}

double PnLCalculator::total_unrealized_pnl() const {
    double total = 0.0;
    for (const auto& [key, s] : states_) {
        total += s.unrealized;
    }
    return total;
}

double PnLCalculator::total_pnl() const {
    return total_realized_pnl() + total_unrealized_pnl();
}

void PnLCalculator::reset() {
    states_.clear();
}

PnLCalculator::SymbolState& PnLCalculator::state(const Symbol& symbol) {
    return states_[symbol.as_key()];
}

const PnLCalculator::SymbolState* PnLCalculator::find_state(
    const Symbol& symbol) const {
    auto it = states_.find(symbol.as_key());
    return (it != states_.end()) ? &it->second : nullptr;
}

}  // namespace qf::strategy
