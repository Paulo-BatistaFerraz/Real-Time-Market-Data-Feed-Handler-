#pragma once

#include <atomic>
#include <memory>
#include <string>
#include "mdfh/simulator/scenario_loader.hpp"
#include "mdfh/simulator/order_generator.hpp"
#include "mdfh/simulator/price_walk.hpp"
#include "mdfh/simulator/sim_order_book.hpp"
#include "mdfh/simulator/sim_matching_engine.hpp"

namespace mdfh::simulator {

struct SimStats {
    uint64_t total_messages = 0;
    uint64_t total_batches  = 0;
    double   actual_rate    = 0.0;
    double   duration_sec   = 0.0;
};

class MarketSimulator {
public:
    explicit MarketSimulator(const SimConfig& config);

    // Run the simulator (blocks until duration expires or stop() called)
    void run();

    // Signal stop from another thread
    void stop();

    const SimStats& stats() const { return stats_; }

private:
    SimConfig config_;
    std::atomic<bool> running_{false};
    SimStats stats_;

    SimOrderBook book_;
    std::vector<PriceWalk> price_walks_;
    std::unique_ptr<OrderGenerator> generator_;
    std::unique_ptr<SimMatchingEngine> matching_engine_;
};

}  // namespace mdfh::simulator
