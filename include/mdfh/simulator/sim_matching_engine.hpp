#pragma once

#include <vector>
#include "mdfh/protocol/messages.hpp"
#include "mdfh/simulator/sim_order_book.hpp"

namespace mdfh::simulator {

class SimMatchingEngine {
public:
    explicit SimMatchingEngine(SimOrderBook& book);

    // After adding an order, check if best_bid >= best_ask
    // If so, generate TradeMessages and update the book
    std::vector<TradeMessage> try_match(const Symbol& symbol);

private:
    SimOrderBook& book_;
};

}  // namespace mdfh::simulator
