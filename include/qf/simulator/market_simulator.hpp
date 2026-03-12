#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <asio.hpp>
#include "qf/simulator/scenario_loader.hpp"
#include "qf/simulator/order_generator.hpp"
#include "qf/simulator/price_walk.hpp"
#include "qf/simulator/sim_order_book.hpp"
#include "qf/simulator/sim_matching_engine.hpp"
#include "qf/simulator/volatility_regime.hpp"
#include "qf/simulator/news_event_generator.hpp"
#include "qf/protocol/batcher.hpp"
#include "qf/network/multicast_sender.hpp"

namespace qf::simulator {

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

    // Signal stop from another thread / signal handler
    void stop();

    const SimStats& stats() const { return stats_; }

private:
    void print_summary() const;

    SimConfig config_;
    std::atomic<bool> running_{false};
    SimStats stats_;

    void apply_news_event(const NewsEvent& event);

    // Owned components
    SimOrderBook book_;
    std::vector<PriceWalk> price_walks_;
    std::vector<double> base_volatilities_;
    std::unique_ptr<OrderGenerator> generator_;
    std::unique_ptr<SimMatchingEngine> matching_engine_;
    VolatilityRegime volatility_regime_;
    NewsEventGenerator news_generator_;
    protocol::Batcher batcher_;
    asio::io_context io_ctx_;
    std::unique_ptr<network::MulticastSender> sender_;
};

}  // namespace qf::simulator
