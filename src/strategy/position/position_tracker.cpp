#include "qf/strategy/position/position_tracker.hpp"

namespace qf::strategy {

void PositionTracker::update(const Symbol& symbol, Side side, Quantity quantity) {
    const auto key = symbol.as_key();
    const auto delta = static_cast<int64_t>(quantity);
    if (side == Side::Buy) {
        positions_[key] += delta;
    } else {
        positions_[key] -= delta;
    }
}

int64_t PositionTracker::get(const Symbol& symbol) const {
    auto it = positions_.find(symbol.as_key());
    return (it != positions_.end()) ? it->second : 0;
}

const std::unordered_map<uint64_t, int64_t>& PositionTracker::get_all() const {
    return positions_;
}

void PositionTracker::reset() {
    positions_.clear();
}

}  // namespace qf::strategy
