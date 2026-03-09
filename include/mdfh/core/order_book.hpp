#pragma once

#include <map>
#include <functional>
#include <vector>
#include <optional>
#include "mdfh/common/types.hpp"
#include "mdfh/core/price_level.hpp"
#include "mdfh/core/order.hpp"
#include "mdfh/core/order_store.hpp"

namespace mdfh::core {

struct DepthLevel {
    Price    price;
    Quantity quantity;
    uint32_t order_count;
};

class OrderBook {
public:
    explicit OrderBook(Symbol symbol);

    // Core operations (return false on failure)
    bool add_order(OrderId id, Side side, Price price, Quantity qty, uint64_t timestamp);
    bool cancel_order(OrderId id);
    bool execute_order(OrderId id, Quantity exec_qty);
    bool replace_order(OrderId id, Price new_price, Quantity new_qty);

    // Top of book
    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    double spread() const;

    // Depth: top N levels
    std::vector<DepthLevel> bid_depth(size_t n) const;
    std::vector<DepthLevel> ask_depth(size_t n) const;

    // Quantities at best
    Quantity best_bid_qty() const;
    Quantity best_ask_qty() const;

    const Symbol& symbol() const { return symbol_; }
    size_t order_count() const { return store_.size(); }

private:
    Symbol symbol_;
    OrderStore store_;

    // Bids: highest price first (std::greater)
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    // Asks: lowest price first (default)
    std::map<Price, PriceLevel> asks_;

    void add_to_side(Side side, Price price, Quantity qty);
    void remove_from_side(Side side, Price price, Quantity qty);
};

}  // namespace mdfh::core
