#include "qf/simulator/sim_matching_engine.hpp"
#include <algorithm>

namespace qf::simulator {

SimMatchingEngine::SimMatchingEngine(SimOrderBook& book) : book_(book) {}

std::vector<TradeMessage> SimMatchingEngine::try_match(const Symbol& symbol) {
    std::vector<TradeMessage> trades;

    while (true) {
        auto bid_id = book_.best_bid_order();
        auto ask_id = book_.best_ask_order();
        if (!bid_id || !ask_id) break;

        const core::Order* bid = book_.find(*bid_id);
        const core::Order* ask = book_.find(*ask_id);
        if (!bid || !ask) break;

        if (bid->price < ask->price) break;

        // Crossing detected — generate a trade
        Quantity match_qty = std::min(bid->remaining_quantity, ask->remaining_quantity);
        Price trade_price = ask->price;  // aggressor pays passive price

        TradeMessage trade{};
        trade.symbol = symbol;
        trade.price = trade_price;
        trade.quantity = match_qty;
        trade.buy_order_id = *bid_id;
        trade.sell_order_id = *ask_id;
        trades.push_back(trade);

        // Execute both sides
        book_.execute(*bid_id, match_qty);
        book_.execute(*ask_id, match_qty);
    }

    return trades;
}

}  // namespace qf::simulator
