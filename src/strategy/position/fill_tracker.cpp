#include "qf/strategy/position/fill_tracker.hpp"

namespace qf::strategy {

const std::vector<Fill> FillTracker::empty_;

void FillTracker::record(Timestamp ts, const Symbol& symbol, Side side,
                         Price price, Quantity quantity) {
    fills_[symbol.as_key()].push_back(Fill{ts, symbol, side, price, quantity});
}

double FillTracker::average_price(const Symbol& symbol) const {
    auto it = fills_.find(symbol.as_key());
    if (it == fills_.end() || it->second.empty()) {
        return 0.0;
    }

    uint64_t total_value = 0;
    uint64_t total_qty   = 0;
    for (const auto& f : it->second) {
        total_value += static_cast<uint64_t>(f.price) * f.quantity;
        total_qty   += f.quantity;
    }

    if (total_qty == 0) return 0.0;
    return static_cast<double>(total_value) / static_cast<double>(total_qty);
}

const std::vector<Fill>& FillTracker::fills(const Symbol& symbol) const {
    auto it = fills_.find(symbol.as_key());
    return (it != fills_.end()) ? it->second : empty_;
}

size_t FillTracker::total_fills() const {
    size_t count = 0;
    for (const auto& [key, vec] : fills_) {
        count += vec.size();
    }
    return count;
}

void FillTracker::reset() {
    fills_.clear();
}

}  // namespace qf::strategy
