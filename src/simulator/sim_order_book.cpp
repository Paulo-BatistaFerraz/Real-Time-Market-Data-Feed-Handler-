#include "qf/simulator/sim_order_book.hpp"
#include <algorithm>
#include <limits>

namespace qf::simulator {

bool SimOrderBook::add(const core::Order& order) {
    if (orders_.count(order.order_id)) {
        return false;
    }
    orders_.emplace(order.order_id, order);
    order_ids_.push_back(order.order_id);
    return true;
}

std::optional<core::Order> SimOrderBook::cancel(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return std::nullopt;
    }
    core::Order order = it->second;
    orders_.erase(it);
    rebuild_ids();
    return order;
}

std::optional<core::Order> SimOrderBook::execute(OrderId id, Quantity qty) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return std::nullopt;
    }
    core::Order& order = it->second;
    if (qty >= order.remaining_quantity) {
        core::Order filled = order;
        filled.remaining_quantity = 0;
        orders_.erase(it);
        rebuild_ids();
        return filled;
    }
    order.remaining_quantity -= qty;
    return order;
}

bool SimOrderBook::replace(OrderId id, Price new_price, Quantity new_qty) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    it->second.price = new_price;
    it->second.remaining_quantity = new_qty;
    return true;
}

std::optional<OrderId> SimOrderBook::random_live_order(std::mt19937& rng) const {
    if (order_ids_.empty()) {
        return std::nullopt;
    }
    std::uniform_int_distribution<size_t> dist(0, order_ids_.size() - 1);
    return order_ids_[dist(rng)];
}

std::optional<OrderId> SimOrderBook::random_live_order(Side side, std::mt19937& rng) const {
    std::vector<OrderId> filtered;
    for (const auto& [id, order] : orders_) {
        if (order.side == side) {
            filtered.push_back(id);
        }
    }
    if (filtered.empty()) {
        return std::nullopt;
    }
    std::uniform_int_distribution<size_t> dist(0, filtered.size() - 1);
    return filtered[dist(rng)];
}

std::optional<Price> SimOrderBook::best_bid() const {
    std::optional<Price> best;
    for (const auto& [id, order] : orders_) {
        if (order.side == Side::Buy) {
            if (!best || order.price > *best) {
                best = order.price;
            }
        }
    }
    return best;
}

std::optional<Price> SimOrderBook::best_ask() const {
    std::optional<Price> best;
    for (const auto& [id, order] : orders_) {
        if (order.side == Side::Sell) {
            if (!best || order.price < *best) {
                best = order.price;
            }
        }
    }
    return best;
}

std::optional<OrderId> SimOrderBook::best_bid_order() const {
    std::optional<OrderId> best_id;
    Price best_price = 0;
    for (const auto& [id, order] : orders_) {
        if (order.side == Side::Buy) {
            if (!best_id || order.price > best_price) {
                best_price = order.price;
                best_id = id;
            }
        }
    }
    return best_id;
}

std::optional<OrderId> SimOrderBook::best_ask_order() const {
    std::optional<OrderId> best_id;
    Price best_price = std::numeric_limits<Price>::max();
    for (const auto& [id, order] : orders_) {
        if (order.side == Side::Sell) {
            if (!best_id || order.price < best_price) {
                best_price = order.price;
                best_id = id;
            }
        }
    }
    return best_id;
}

const core::Order* SimOrderBook::find(OrderId id) const {
    auto it = orders_.find(id);
    if (it == orders_.end()) return nullptr;
    return &it->second;
}

void SimOrderBook::rebuild_ids() {
    order_ids_.clear();
    for (const auto& [id, _] : orders_) {
        order_ids_.push_back(id);
    }
}

}  // namespace qf::simulator
