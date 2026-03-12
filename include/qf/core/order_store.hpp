#pragma once

#include <optional>
#include <unordered_map>
#include "qf/core/order.hpp"
#include "qf/common/constants.hpp"

namespace qf::core {

class OrderStore {
public:
    explicit OrderStore(size_t initial_capacity = ORDER_STORE_INITIAL_CAPACITY);

    // Add a new order. Returns false if order_id already exists.
    bool add(const Order& order);

    // Find order by id. Returns nullptr if not found.
    Order* find(OrderId id);
    const Order* find(OrderId id) const;

    // Remove order by id. Returns the removed order, or nullopt if not found.
    std::optional<Order> remove(OrderId id);

    // Update price and quantity for an existing order
    bool update_price_qty(OrderId id, Price new_price, Quantity new_qty);

    // Reduce quantity (for partial executions)
    bool reduce_quantity(OrderId id, Quantity amount);

    // Pre-allocate buckets to avoid rehashing on hot path
    void reserve(size_t n) { orders_.reserve(n); }

    size_t size() const { return orders_.size(); }
    bool empty() const { return orders_.empty(); }
    void clear() { orders_.clear(); }

private:
    std::unordered_map<OrderId, Order> orders_;
};

}  // namespace qf::core
