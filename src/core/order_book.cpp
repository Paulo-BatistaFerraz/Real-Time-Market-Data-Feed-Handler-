#include "mdfh/core/order_book.hpp"

namespace mdfh::core {

// TODO: Implement order book logic

OrderBook::OrderBook(Symbol symbol) : symbol_(symbol) {}

bool OrderBook::add_order(OrderId id, Side side, Price price, Quantity qty, uint64_t timestamp) {
    (void)id; (void)side; (void)price; (void)qty; (void)timestamp;
    return false;
}

bool OrderBook::cancel_order(OrderId id) {
    (void)id;
    return false;
}

bool OrderBook::execute_order(OrderId id, Quantity exec_qty) {
    (void)id; (void)exec_qty;
    return false;
}

bool OrderBook::replace_order(OrderId id, Price new_price, Quantity new_qty) {
    (void)id; (void)new_price; (void)new_qty;
    return false;
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

std::vector<DepthLevel> OrderBook::bid_depth(size_t n) const {
    (void)n;
    return {};
}

std::vector<DepthLevel> OrderBook::ask_depth(size_t n) const {
    (void)n;
    return {};
}

Quantity OrderBook::best_bid_qty() const {
    if (bids_.empty()) return 0;
    return bids_.begin()->second.total_qty;
}

Quantity OrderBook::best_ask_qty() const {
    if (asks_.empty()) return 0;
    return asks_.begin()->second.total_qty;
}

void OrderBook::add_to_side(Side side, Price price, Quantity qty) {
    (void)side; (void)price; (void)qty;
}

void OrderBook::remove_from_side(Side side, Price price, Quantity qty) {
    (void)side; (void)price; (void)qty;
}

}  // namespace mdfh::core
