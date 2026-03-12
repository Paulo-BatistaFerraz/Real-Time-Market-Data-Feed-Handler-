#include "qf/core/order_store.hpp"

namespace qf::core {

OrderStore::OrderStore(size_t initial_capacity) {
    orders_.reserve(initial_capacity);
}

bool OrderStore::add(const Order& order) {
    auto [it, inserted] = orders_.emplace(order.order_id, order);
    return inserted;
}

Order* OrderStore::find(OrderId id) {
    auto it = orders_.find(id);
    return it != orders_.end() ? &it->second : nullptr;
}

const Order* OrderStore::find(OrderId id) const {
    auto it = orders_.find(id);
    return it != orders_.end() ? &it->second : nullptr;
}

std::optional<Order> OrderStore::remove(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return std::nullopt;
    }
    Order removed = std::move(it->second);
    orders_.erase(it);
    return removed;
}

bool OrderStore::update_price_qty(OrderId id, Price new_price, Quantity new_qty) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;
    it->second.price = new_price;
    it->second.remaining_quantity = new_qty;
    return true;
}

bool OrderStore::reduce_quantity(OrderId id, Quantity amount) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;
    if (amount > it->second.remaining_quantity) return false;
    it->second.remaining_quantity -= amount;
    return true;
}

}  // namespace qf::core
