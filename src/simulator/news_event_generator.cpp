#include "qf/simulator/news_event_generator.hpp"

namespace qf::simulator {

NewsEventGenerator::NewsEventGenerator(uint32_t seed, size_t num_symbols,
                                       const NewsConfig& config)
    : config_(config)
    , num_symbols_(num_symbols)
    , rng_(seed)
    , magnitude_dist_({config.small_prob, config.medium_prob, config.large_prob}) {}

std::optional<NewsEvent> NewsEventGenerator::check() {
    ++ticks_since_check_;
    if (ticks_since_check_ < config_.check_interval) {
        return std::nullopt;
    }
    ticks_since_check_ = 0;

    // Roll for event occurrence
    if (uniform_(rng_) >= config_.event_probability) {
        return std::nullopt;
    }

    NewsEvent event;

    // Magnitude
    int mag = magnitude_dist_(rng_);
    event.magnitude = static_cast<EventMagnitude>(mag);

    // Direction (50/50)
    event.direction = (uniform_(rng_) < 0.5)
        ? EventDirection::Up
        : EventDirection::Down;

    // Scope
    event.scope = (uniform_(rng_) < config_.systemic_prob)
        ? EventScope::Systemic
        : EventScope::Single;

    // Target symbol (only used when scope == Single)
    if (num_symbols_ > 0) {
        std::uniform_int_distribution<size_t> sym_dist(0, num_symbols_ - 1);
        event.symbol_index = sym_dist(rng_);
    } else {
        event.symbol_index = 0;
    }

    // Compute signed price jump fraction
    double base_jump = 0.0;
    switch (event.magnitude) {
        case EventMagnitude::Small:  base_jump = config_.small_jump;  break;
        case EventMagnitude::Medium: base_jump = config_.medium_jump; break;
        case EventMagnitude::Large:  base_jump = config_.large_jump;  break;
    }
    event.price_jump = (event.direction == EventDirection::Up) ? base_jump : -base_jump;

    ++total_events_;
    return event;
}

}  // namespace qf::simulator
