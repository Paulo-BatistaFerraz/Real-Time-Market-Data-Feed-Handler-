#include "qf/core/order_book.hpp"

namespace qf::core {

OrderBook::OrderBook(Symbol symbol) : symbol_(symbol) {}

BookSnapshot OrderBook::make_update(uint64_t timestamp) const {
    return BookSnapshot{symbol_, best_bid(), best_ask(), timestamp};
}

std::optional<BookSnapshot> OrderBook::add_order(OrderId id, Side side, Price price, Quantity qty, uint64_t timestamp) {
    Order order(id, symbol_, side, price, qty, timestamp);
    if (!store_.add(order)) {
        return std::nullopt;  // duplicate order_id
    }
    add_to_side(side, price, qty);
    return make_update(timestamp);
}

std::optional<BookSnapshot> OrderBook::cancel_order(OrderId id) {
    auto removed = store_.remove(id);
    if (!removed) {
        return std::nullopt;
    }
    remove_from_side(removed->side, removed->price, removed->remaining_quantity);
    return make_update(removed->timestamp);
}

std::optional<BookSnapshot> OrderBook::execute_order(OrderId id, Quantity exec_qty) {
    Order* order = store_.find(id);
    if (!order || exec_qty == 0 || exec_qty > order->remaining_quantity) {
        return std::nullopt;
    }

    Side side = order->side;
    Price price = order->price;
    uint64_t ts = order->timestamp;

    order->remaining_quantity -= exec_qty;

    // Update the price level
    auto update_level = [&](auto& side_map) {
        auto it = side_map.find(price);
        if (it != side_map.end()) {
            it->second.total_quantity -= exec_qty;
            if (order->remaining_quantity == 0) {
                // Fully filled — remove order and decrement count
                it->second.order_count--;
                if (it->second.is_empty()) {
                    side_map.erase(it);
                }
                store_.remove(id);
            }
        }
    };

    if (side == Side::Buy) {
        update_level(bids_);
    } else {
        update_level(asks_);
    }

    return make_update(ts);
}

std::optional<BookSnapshot> OrderBook::replace_order(OrderId id, Price new_price, Quantity new_qty) {
    Order* order = store_.find(id);
    if (!order) {
        return std::nullopt;
    }

    Side side = order->side;
    Price old_price = order->price;
    Quantity old_qty = order->remaining_quantity;
    uint64_t ts = order->timestamp;

    // Remove from old price level
    remove_from_side(side, old_price, old_qty);

    // Update the order in the store
    order->price = new_price;
    order->remaining_quantity = new_qty;

    // Add to new price level
    add_to_side(side, new_price, new_qty);

    return make_update(ts);
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

double OrderBook::spread() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) return 0.0;
    return price_to_double(*ask) - price_to_double(*bid);
}

std::vector<DepthLevel> OrderBook::top_n_levels(Side side, size_t n) const {
    if (side == Side::Buy) {
        return bid_depth(n);
    }
    return ask_depth(n);
}

std::vector<DepthLevel> OrderBook::bid_depth(size_t n) const {
    std::vector<DepthLevel> result;
    result.reserve(n);
    size_t count = 0;
    for (auto it = bids_.begin(); it != bids_.end() && count < n; ++it, ++count) {
        result.push_back({it->first, it->second.total_quantity, it->second.order_count});
    }
    return result;
}

std::vector<DepthLevel> OrderBook::ask_depth(size_t n) const {
    std::vector<DepthLevel> result;
    result.reserve(n);
    size_t count = 0;
    for (auto it = asks_.begin(); it != asks_.end() && count < n; ++it, ++count) {
        result.push_back({it->first, it->second.total_quantity, it->second.order_count});
    }
    return result;
}

Quantity OrderBook::best_bid_qty() const {
    if (bids_.empty()) return 0;
    return bids_.begin()->second.total_quantity;
}

Quantity OrderBook::best_ask_qty() const {
    if (asks_.empty()) return 0;
    return asks_.begin()->second.total_quantity;
}

void OrderBook::add_to_side(Side side, Price price, Quantity qty) {
    if (side == Side::Buy) {
        auto& level = bids_[price];
        if (level.price == 0) level.price = price;
        level.add(qty);
    } else {
        auto& level = asks_[price];
        if (level.price == 0) level.price = price;
        level.add(qty);
    }
}

void OrderBook::remove_from_side(Side side, Price price, Quantity qty) {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            if (it->second.remove(qty)) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            if (it->second.remove(qty)) {
                asks_.erase(it);
            }
        }
    }
}

}  // namespace qf::core
