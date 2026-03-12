#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include "qf/common/types.hpp"

namespace qf::simulator {

enum class EventMagnitude : uint8_t {
    Small  = 0,
    Medium = 1,
    Large  = 2
};

enum class EventDirection : uint8_t {
    Up   = 0,
    Down = 1
};

enum class EventScope : uint8_t {
    Single   = 0,   // Affects one symbol
    Systemic = 1    // Affects all symbols
};

struct NewsEvent {
    EventMagnitude magnitude;
    EventDirection direction;
    EventScope     scope;
    size_t         symbol_index;  // Only meaningful when scope == Single
    double         price_jump;    // Signed jump as fraction of current price
};

struct NewsConfig {
    // Probability of an event firing per check
    double event_probability = 0.002;

    // How many simulation ticks between event checks
    uint32_t check_interval = 500;

    // Magnitude probabilities (must sum to 1.0)
    double small_prob  = 0.70;
    double medium_prob = 0.25;
    double large_prob  = 0.05;

    // Price jump fractions for each magnitude
    double small_jump  = 0.005;   // 0.5%
    double medium_jump = 0.02;    // 2%
    double large_jump  = 0.08;    // 8%

    // Probability that event is systemic (affects all symbols)
    double systemic_prob = 0.20;
};

class NewsEventGenerator {
public:
    NewsEventGenerator(uint32_t seed, size_t num_symbols,
                       const NewsConfig& config = {});

    // Check if a news event fires this tick.
    // Returns the event if one fired, nullopt otherwise.
    std::optional<NewsEvent> check();

    uint64_t total_events() const { return total_events_; }

private:
    NewsConfig config_;
    size_t num_symbols_;
    uint32_t ticks_since_check_ = 0;
    uint64_t total_events_ = 0;

    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_{0.0, 1.0};
    std::discrete_distribution<int> magnitude_dist_;
};

}  // namespace qf::simulator
