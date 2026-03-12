#pragma once

#include <map>
#include <functional>
#include <vector>
#include <optional>
#include "qf/common/types.hpp"
#include "qf/core/price_level.hpp"
#include "qf/core/order.hpp"
#include "qf/core/order_store.hpp"

namespace qf::core {

struct DepthLevel {
    Price    price;
    Quantity quantity;
    uint32_t order_count;
};

struct BookSnapshot {
    Symbol                symbol;
    std::optional<Price>  best_bid;
    std::optional<Price>  best_ask;
    uint64_t              timestamp;
};

class OrderBook {
public:
    explicit OrderBook(Symbol symbol);

    // Core operations — return BookUpdate on success, nullopt on failure
    std::optional<BookSnapshot> add_order(OrderId id, Side side, Price price, Quantity qty, uint64_t timestamp);
    std::optional<BookSnapshot> cancel_order(OrderId id);
    std::optional<BookSnapshot> execute_order(OrderId id, Quantity exec_qty);
    std::optional<BookSnapshot> replace_order(OrderId id, Price new_price, Quantity new_qty);

    // Top of book
    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    double spread() const;

    // Depth: top N levels by side
    std::vector<DepthLevel> top_n_levels(Side side, size_t n) const;
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
    BookSnapshot make_update(uint64_t timestamp) const;
};

}  // namespace qf::core
