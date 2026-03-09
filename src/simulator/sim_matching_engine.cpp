#include "mdfh/simulator/sim_matching_engine.hpp"

namespace mdfh::simulator {

// TODO: Implement matching logic

SimMatchingEngine::SimMatchingEngine(SimOrderBook& book) : book_(book) {}

std::vector<TradeMessage> SimMatchingEngine::try_match(const Symbol& symbol) {
    (void)symbol;
    return {};
}

}  // namespace mdfh::simulator
