#pragma once

#include <unordered_map>
#include <vector>
#include <optional>
#include <random>
#include "qf/common/types.hpp"
#include "qf/core/order.hpp"

namespace qf::simulator {

// Simulator's internal order book for validation
// Ensures Cancel/Execute only reference existing orders
class SimOrderBook {
public:
    SimOrderBook() = default;

    bool add(const core::Order& order);
    std::optional<core::Order> cancel(OrderId id);
    std::optional<core::Order> execute(OrderId id, Quantity qty);
    bool replace(OrderId id, Price new_price, Quantity new_qty);

    // Pick a random live order (for generating Cancel/Execute)
    std::optional<OrderId> random_live_order(std::mt19937& rng) const;

    // Pick a random live order on a specific side
    std::optional<OrderId> random_live_order(Side side, std::mt19937& rng) const;

    size_t live_order_count() const { return orders_.size(); }
    bool empty() const { return orders_.empty(); }
    void clear() { orders_.clear(); order_ids_.clear(); }

    // Best bid/ask for matching
    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;

    // Find order at best bid/ask (for matching engine)
    std::optional<OrderId> best_bid_order() const;
    std::optional<OrderId> best_ask_order() const;

    // Look up an order by ID
    const core::Order* find(OrderId id) const;

private:
    std::unordered_map<OrderId, core::Order> orders_;
    std::vector<OrderId> order_ids_;  // for random access
    void rebuild_ids();
};

}  // namespace qf::simulator
