#include "qf/core/book_manager.hpp"
#include "qf/common/clock.hpp"

namespace qf::core {

BookUpdate BookManager::process(const ParsedEvent& event) {
    uint64_t recv_ts = event.receive_timestamp;

    return std::visit([&](const auto& msg) -> BookUpdate {
        using T = std::decay_t<decltype(msg)>;

        if constexpr (std::is_same_v<T, AddOrder>) {
            auto& book = get_or_create(msg.symbol);
            order_to_symbol_[msg.order_id] = msg.symbol.as_key();
            book.add_order(msg.order_id, msg.side, msg.price, msg.quantity, 0);
            return make_update(book, recv_ts);

        } else if constexpr (std::is_same_v<T, CancelOrder>) {
            auto it = order_to_symbol_.find(msg.order_id);
            if (it == order_to_symbol_.end()) {
                return BookUpdate{};
            }
            auto book_it = books_.find(it->second);
            if (book_it == books_.end()) {
                return BookUpdate{};
            }
            auto& book = *book_it->second;
            book.cancel_order(msg.order_id);
            order_to_symbol_.erase(it);
            return make_update(book, recv_ts);

        } else if constexpr (std::is_same_v<T, ExecuteOrder>) {
            auto it = order_to_symbol_.find(msg.order_id);
            if (it == order_to_symbol_.end()) {
                return BookUpdate{};
            }
            auto book_it = books_.find(it->second);
            if (book_it == books_.end()) {
                return BookUpdate{};
            }
            auto& book = *book_it->second;
            book.execute_order(msg.order_id, msg.exec_quantity);
            return make_update(book, recv_ts);

        } else if constexpr (std::is_same_v<T, ReplaceOrder>) {
            auto it = order_to_symbol_.find(msg.order_id);
            if (it == order_to_symbol_.end()) {
                return BookUpdate{};
            }
            auto book_it = books_.find(it->second);
            if (book_it == books_.end()) {
                return BookUpdate{};
            }
            auto& book = *book_it->second;
            book.replace_order(msg.order_id, msg.new_price, msg.new_quantity);
            return make_update(book, recv_ts);

        } else if constexpr (std::is_same_v<T, TradeMessage>) {
            auto& book = get_or_create(msg.symbol);
            return make_update(book, recv_ts);

        } else {
            return BookUpdate{};
        }
    }, event.message);
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

std::vector<Symbol> BookManager::get_all_symbols() const {
    std::vector<Symbol> result;
    result.reserve(books_.size());
    for (const auto& [key, book] : books_) {
        result.push_back(book->symbol());
    }
    return result;
}

std::vector<BookUpdate> BookManager::get_snapshot() const {
    std::vector<BookUpdate> snapshot;
    snapshot.reserve(books_.size());
    uint64_t now = Clock::now_ns();
    for (const auto& [key, book] : books_) {
        snapshot.push_back(make_update(*book, now));
    }
    return snapshot;
}

void BookManager::reset() {
    books_.clear();
    order_to_symbol_.clear();
}

OrderBook& BookManager::get_or_create(const Symbol& symbol) {
    auto key = symbol.as_key();
    auto it = books_.find(key);
    if (it == books_.end()) {
        auto [inserted, ok] = books_.emplace(key, std::make_unique<OrderBook>(symbol));
        return *inserted->second;
    }
    return *it->second;
}

BookUpdate BookManager::make_update(const OrderBook& book, uint64_t recv_timestamp) const {
    BookUpdate update{};
    update.symbol = book.symbol();

    auto bid_price = book.best_bid();
    if (bid_price.has_value()) {
        PriceLevel lvl(*bid_price);
        lvl.total_quantity = book.best_bid_qty();
        lvl.order_count = 1;  // aggregated level
        update.best_bid = lvl;
    }

    auto ask_price = book.best_ask();
    if (ask_price.has_value()) {
        PriceLevel lvl(*ask_price);
        lvl.total_quantity = book.best_ask_qty();
        lvl.order_count = 1;  // aggregated level
        update.best_ask = lvl;
    }

    update.receive_timestamp = recv_timestamp;
    update.book_timestamp = Clock::now_ns();
    return update;
}

}  // namespace qf::core
