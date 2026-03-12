#pragma once

#include <array>
#include <cstdint>
#include <random>

namespace qf::simulator {

enum class RegimeState : uint8_t {
    Calm     = 0,
    Volatile = 1,
    Crash    = 2
};

// Per-state parameters: multipliers applied to base volatility and message rate
struct RegimeParams {
    double volatility_multiplier = 1.0;
    double rate_multiplier       = 1.0;
};

// 3x3 row-stochastic Markov transition matrix
// transition_matrix[from][to] = probability of transitioning from -> to
using TransitionMatrix = std::array<std::array<double, 3>, 3>;

struct RegimeConfig {
    TransitionMatrix matrix = {{
        // From Calm:     stay Calm 0.97, go Volatile 0.025, go Crash 0.005
        {{ 0.970, 0.025, 0.005 }},
        // From Volatile: go Calm 0.05,  stay Volatile 0.90, go Crash 0.05
        {{ 0.050, 0.900, 0.050 }},
        // From Crash:    go Calm 0.10,  go Volatile 0.20,  stay Crash 0.70
        {{ 0.100, 0.200, 0.700 }}
    }};

    // Parameters for each state
    std::array<RegimeParams, 3> params = {{
        { 1.0, 1.0 },   // Calm
        { 3.0, 1.5 },   // Volatile
        { 6.0, 2.0 }    // Crash
    }};

    // How many simulation ticks between transition checks
    uint32_t check_interval = 1000;
};

class VolatilityRegime {
public:
    explicit VolatilityRegime(uint32_t seed, const RegimeConfig& config = {});

    // Called each simulation tick; may trigger a state transition
    void update();

    RegimeState state() const { return state_; }

    // Current multipliers
    double volatility_multiplier() const;
    double rate_multiplier() const;

    // Allow runtime reconfiguration of the transition matrix
    void set_transition_matrix(const TransitionMatrix& matrix);

private:
    void try_transition();

    RegimeState state_ = RegimeState::Calm;
    RegimeConfig config_;
    uint32_t ticks_since_check_ = 0;

    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_{0.0, 1.0};
};

}  // namespace qf::simulator
