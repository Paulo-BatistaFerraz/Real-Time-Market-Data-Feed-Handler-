#include "mdfh/simulator/sim_order_book.hpp"
#include <algorithm>

namespace mdfh::simulator {

// TODO: Implement simulator order book logic

bool SimOrderBook::add(const core::Order& order) {
    (void)order;
    return false;
}

std::optional<core::Order> SimOrderBook::cancel(OrderId id) {
    (void)id;
    return std::nullopt;
}

std::optional<core::Order> SimOrderBook::execute(OrderId id, Quantity qty) {
    (void)id; (void)qty;
    return std::nullopt;
}

bool SimOrderBook::replace(OrderId id, Price new_price, Quantity new_qty) {
    (void)id; (void)new_price; (void)new_qty;
    return false;
}

std::optional<OrderId> SimOrderBook::random_live_order(std::mt19937& rng) const {
    (void)rng;
    return std::nullopt;
}

std::optional<OrderId> SimOrderBook::random_live_order(Side side, std::mt19937& rng) const {
    (void)side; (void)rng;
    return std::nullopt;
}

std::optional<Price> SimOrderBook::best_bid() const {
    return std::nullopt;
}

std::optional<Price> SimOrderBook::best_ask() const {
    return std::nullopt;
}

void SimOrderBook::rebuild_ids() {
    order_ids_.clear();
    for (const auto& [id, _] : orders_) {
        order_ids_.push_back(id);
    }
}

}  // namespace mdfh::simulator
