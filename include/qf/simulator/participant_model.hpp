#pragma once

#include <cstdint>
#include <random>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/simulator/price_walk.hpp"
#include "qf/simulator/sim_order_book.hpp"

namespace qf::simulator {

enum class TraderType : uint8_t {
    MarketMaker    = 0,   // Quotes both sides, tight spreads, high cancel rate
    MomentumTrader = 1,   // Follows trend, one-sided, moderate cancel rate
    RandomTrader   = 2    // Noise trader, uniform behavior
};

// Per-trader-type behavioral parameters
struct TraderProfile {
    TraderType type;

    // Order size distribution (log-normal parameters)
    double size_mean    = 100.0;
    double size_stddev  = 50.0;
    uint32_t min_qty    = 1;
    uint32_t max_qty    = 5000;

    // Message type weights
    double add_weight     = 0.40;
    double cancel_weight  = 0.25;
    double execute_weight = 0.20;
    double replace_weight = 0.10;
    double trade_weight   = 0.05;

    // Placement: how far from mid-price orders are placed (in ticks)
    double spread_ticks_mean   = 5.0;
    double spread_ticks_stddev = 2.0;

    // Cancel rate: probability of canceling own orders each tick
    double cancel_rate = 0.10;
};

// Configuration for the participant model
struct ParticipantConfig {
    uint32_t num_market_makers    = 2;
    uint32_t num_momentum_traders = 3;
    uint32_t num_random_traders   = 5;

    // Override defaults per type (optional)
    TraderProfile market_maker_profile;
    TraderProfile momentum_profile;
    TraderProfile random_profile;
};

TraderProfile default_market_maker_profile();
TraderProfile default_momentum_profile();
TraderProfile default_random_profile();
ParticipantConfig default_participant_config();

// Represents a single simulated participant
class Participant {
public:
    Participant(uint32_t id, const TraderProfile& profile,
                const std::vector<Symbol>& symbols,
                std::vector<PriceWalk>& price_walks,
                SimOrderBook& book,
                uint32_t seed);

    // Generate next message from this participant
    // Returns bytes written (0 on failure)
    size_t next(uint8_t* buffer, size_t buffer_size);

    uint32_t id() const { return id_; }
    TraderType type() const { return profile_.type; }
    uint64_t total_generated() const { return total_generated_; }
    const TraderProfile& profile() const { return profile_; }

private:
    uint32_t id_;
    TraderProfile profile_;
    std::vector<Symbol> symbols_;
    std::vector<PriceWalk>& price_walks_;
    SimOrderBook& book_;

    std::mt19937 rng_;
    std::discrete_distribution<int> type_dist_;
    std::uniform_int_distribution<size_t> symbol_dist_;
    std::normal_distribution<double> size_dist_;

    OrderId next_order_id_;
    uint64_t total_generated_ = 0;

    // Track momentum direction per symbol for MomentumTrader
    std::vector<double> prev_prices_;

    size_t generate_add(uint8_t* buffer, size_t buffer_size);
    size_t generate_cancel(uint8_t* buffer, size_t buffer_size);
    size_t generate_execute(uint8_t* buffer, size_t buffer_size);
    size_t generate_replace(uint8_t* buffer, size_t buffer_size);
    Quantity random_quantity();
    Side choose_side(size_t symbol_idx);
};

// Manages a pool of participants with different archetypes
class ParticipantModel {
public:
    ParticipantModel(const std::vector<Symbol>& symbols,
                     std::vector<PriceWalk>& price_walks,
                     SimOrderBook& book,
                     uint32_t seed,
                     const ParticipantConfig& config = default_participant_config());

    // Select a random participant (weighted by activity level) and generate a message
    size_t next(uint8_t* buffer, size_t buffer_size);

    size_t participant_count() const { return participants_.size(); }
    uint64_t total_generated() const;

    const std::vector<Participant>& participants() const { return participants_; }

private:
    std::vector<Participant> participants_;
    std::mt19937 rng_;
    std::uniform_int_distribution<size_t> participant_dist_;
};

}  // namespace qf::simulator
