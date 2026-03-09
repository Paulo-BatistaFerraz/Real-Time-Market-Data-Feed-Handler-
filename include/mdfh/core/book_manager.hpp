#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include "mdfh/common/types.hpp"
#include "mdfh/core/order_book.hpp"
#include "mdfh/core/pipeline_types.hpp"

namespace mdfh::core {

class BookManager {
public:
    BookManager() = default;

    // Process a parsed event → apply to correct book → return BookUpdate
    BookUpdate process(const ParsedEvent& event);

    // Access individual books
    OrderBook* get_book(const Symbol& symbol);
    const OrderBook* get_book(const Symbol& symbol) const;

    // List all tracked symbols
    std::vector<Symbol> all_symbols() const;

    size_t book_count() const { return books_.size(); }

    void reset();

private:
    std::unordered_map<uint64_t, std::unique_ptr<OrderBook>> books_;

    // Get or create book for symbol
    OrderBook& get_or_create(const Symbol& symbol);

    // Build BookUpdate from current book state
    BookUpdate make_update(const OrderBook& book, EventType event, uint64_t recv_timestamp);
};

}  // namespace mdfh::core
