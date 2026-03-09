#include "mdfh/core/order_store.hpp"

namespace mdfh::core {

// TODO: Implement order store logic

OrderStore::OrderStore(size_t initial_capacity) {
    orders_.reserve(initial_capacity);
}

bool OrderStore::add(const Order& order) {
    (void)order;
    return false;
}

Order* OrderStore::find(OrderId id) {
    (void)id;
    return nullptr;
}

const Order* OrderStore::find(OrderId id) const {
    (void)id;
    return nullptr;
}

bool OrderStore::remove(OrderId id) {
    (void)id;
    return false;
}

bool OrderStore::update_price_qty(OrderId id, Price new_price, Quantity new_qty) {
    (void)id; (void)new_price; (void)new_qty;
    return false;
}

bool OrderStore::reduce_quantity(OrderId id, Quantity amount) {
    (void)id; (void)amount;
    return false;
}

}  // namespace mdfh::core
