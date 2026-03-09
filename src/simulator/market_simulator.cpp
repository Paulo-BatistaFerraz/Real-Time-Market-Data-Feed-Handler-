#include "mdfh/simulator/market_simulator.hpp"
#include <iostream>
#include <chrono>

namespace mdfh::simulator {

// TODO: Implement simulator run loop

MarketSimulator::MarketSimulator(const SimConfig& config) : config_(config) {
    // Create price walks for each symbol
    for (const auto& sym : config.symbols) {
        price_walks_.emplace_back(sym.start_price, sym.volatility, sym.tick_size, config.seed);
    }

    // Build symbol list
    std::vector<Symbol> symbols;
    for (const auto& sym : config.symbols) {
        symbols.emplace_back(sym.name.c_str());
    }

    matching_engine_ = std::make_unique<SimMatchingEngine>(book_);
    generator_ = std::make_unique<OrderGenerator>(symbols, price_walks_, book_, config.seed);
}

void MarketSimulator::run() {
    running_.store(true);
    std::cout << "[SIM] Running...\n";

    // TODO: Main loop
    // - Generate messages at config_.msg_rate per second
    // - Encode → Batch → Send via MulticastSender
    // - Track stats
    // - Stop after config_.duration_sec or when stop() called

    running_.store(false);
}

void MarketSimulator::stop() {
    running_.store(false);
}

}  // namespace mdfh::simulator
