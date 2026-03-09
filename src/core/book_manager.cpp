#include "mdfh/core/book_manager.hpp"
#include "mdfh/common/clock.hpp"

namespace mdfh::core {

// TODO: Implement book manager logic

BookUpdate BookManager::process(const ParsedEvent& event) {
    (void)event;
    return {};
}

OrderBook* BookManager::get_book(const Symbol& symbol) {
    auto it = books_.find(symbol.as_key());
    if (it == books_.end()) return nullptr;
    return it->second.get();
}

const OrderBook* BookManager::get_book(const Symbol& symbol) const {
    auto it = books_.find(symbol.as_key());
    if (it == books_.end()) return nullptr;
    return it->second.get();
}

std::vector<Symbol> BookManager::all_symbols() const {
    std::vector<Symbol> result;
    for (const auto& [key, book] : books_) {
        result.push_back(book->symbol());
    }
    return result;
}

void BookManager::reset() {
    books_.clear();
}

OrderBook& BookManager::get_or_create(const Symbol& symbol) {
    auto key = symbol.as_key();
    auto it = books_.find(key);
    if (it == books_.end()) {
        auto [inserted, _] = books_.emplace(key, std::make_unique<OrderBook>(symbol));
        return *inserted->second;
    }
    return *it->second;
}

BookUpdate BookManager::make_update(const OrderBook& book, EventType event, uint64_t recv_timestamp) {
    (void)book; (void)event; (void)recv_timestamp;
    return {};
}

}  // namespace mdfh::core
