#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include "qf/common/types.hpp"
#include "qf/core/order_book.hpp"
#include "qf/core/pipeline_types.hpp"

namespace qf::core {

class BookManager {
public:
    BookManager() = default;

    // Process a parsed event → apply to correct book → return BookUpdate
    BookUpdate process(const ParsedEvent& event);

    // Access individual books
    OrderBook* get_book(const Symbol& symbol);
    const OrderBook* get_book(const Symbol& symbol) const;

    // List all tracked symbols
    std::vector<Symbol> get_all_symbols() const;

    // Top-of-book snapshot for all symbols (for display)
    std::vector<BookUpdate> get_snapshot() const;

    size_t book_count() const { return books_.size(); }

    void reset();

private:
    std::unordered_map<uint64_t, std::unique_ptr<OrderBook>> books_;
    std::unordered_map<OrderId, uint64_t> order_to_symbol_;

    // Get or create book for symbol
    OrderBook& get_or_create(const Symbol& symbol);

    // Build pipeline BookUpdate from current book state
    BookUpdate make_update(const OrderBook& book, uint64_t recv_timestamp) const;
};

}  // namespace qf::core
