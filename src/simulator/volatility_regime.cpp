#include "qf/simulator/volatility_regime.hpp"

namespace qf::simulator {

VolatilityRegime::VolatilityRegime(uint32_t seed, const RegimeConfig& config)
    : config_(config)
    , rng_(seed) {}

void VolatilityRegime::update() {
    ++ticks_since_check_;
    if (ticks_since_check_ >= config_.check_interval) {
        ticks_since_check_ = 0;
        try_transition();
    }
}

double VolatilityRegime::volatility_multiplier() const {
    return config_.params[static_cast<size_t>(state_)].volatility_multiplier;
}

double VolatilityRegime::rate_multiplier() const {
    return config_.params[static_cast<size_t>(state_)].rate_multiplier;
}

void VolatilityRegime::set_transition_matrix(const TransitionMatrix& matrix) {
    config_.matrix = matrix;
}

void VolatilityRegime::try_transition() {
    size_t from = static_cast<size_t>(state_);
    double roll = uniform_(rng_);

    double cumulative = 0.0;
    for (size_t to = 0; to < 3; ++to) {
        cumulative += config_.matrix[from][to];
        if (roll < cumulative) {
            state_ = static_cast<RegimeState>(to);
            return;
        }
    }
    // Floating-point safety: stay in current state if nothing matched
}

}  // namespace qf::simulator
