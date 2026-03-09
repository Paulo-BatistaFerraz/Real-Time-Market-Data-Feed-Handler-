#pragma once

#include <unordered_map>
#include "mdfh/core/order.hpp"
#include "mdfh/common/constants.hpp"

namespace mdfh::core {

class OrderStore {
public:
    explicit OrderStore(size_t initial_capacity = ORDER_STORE_INITIAL_CAPACITY);

    // Add a new order. Returns false if order_id already exists.
    bool add(const Order& order);

    // Find order by id. Returns nullptr if not found.
    Order* find(OrderId id);
    const Order* find(OrderId id) const;

    // Remove order by id. Returns false if not found.
    bool remove(OrderId id);

    // Update price and quantity for an existing order
    bool update_price_qty(OrderId id, Price new_price, Quantity new_qty);

    // Reduce quantity (for partial executions)
    bool reduce_quantity(OrderId id, Quantity amount);

    size_t size() const { return orders_.size(); }
    bool empty() const { return orders_.empty(); }
    void clear() { orders_.clear(); }

private:
    std::unordered_map<OrderId, Order> orders_;
};

}  // namespace mdfh::core
