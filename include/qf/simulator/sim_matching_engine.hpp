#pragma once

#include <vector>
#include "qf/protocol/messages.hpp"
#include "qf/simulator/sim_order_book.hpp"

namespace qf::simulator {

class SimMatchingEngine {
public:
    explicit SimMatchingEngine(SimOrderBook& book);

    // After adding an order, check if best_bid >= best_ask
    // If so, generate TradeMessages and update the book
    std::vector<TradeMessage> try_match(const Symbol& symbol);

private:
    SimOrderBook& book_;
};

}  // namespace qf::simulator
