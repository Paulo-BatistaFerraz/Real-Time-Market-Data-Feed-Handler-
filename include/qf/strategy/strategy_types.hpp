#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "qf/common/types.hpp"

namespace qf::strategy {

// Order type for strategy decisions
enum class OrderType : uint8_t {
    Market = 0,
    Limit  = 1
};

// Urgency level for order execution
enum class Urgency : uint8_t {
    Low    = 0,
    Medium = 1,
    High   = 2
};

// A trading decision produced by a strategy in response to a signal.
struct Decision {
    Symbol    symbol;
    Side      side;
    Quantity  quantity;
    OrderType order_type;
    Price     limit_price;  // only meaningful when order_type == Limit
    Urgency   urgency;
};

// Minimal portfolio snapshot passed to strategies for position-aware decisions.
struct PortfolioView {
    // Current position per symbol: positive = long, negative = short
    std::unordered_map<std::string, int64_t> positions;

    // Available cash (fixed-point, same scale as Price)
    double cash{0.0};

    int64_t position(const std::string& symbol) const {
        auto it = positions.find(symbol);
        return (it != positions.end()) ? it->second : 0;
    }

    bool has_position(const std::string& symbol) const {
        auto it = positions.find(symbol);
        return it != positions.end() && it->second != 0;
    }
};

}  // namespace qf::strategy
