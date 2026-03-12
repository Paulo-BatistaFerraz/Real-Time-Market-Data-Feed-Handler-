#pragma once

#include <cstdint>
#include <random>
#include "qf/common/types.hpp"

namespace qf::simulator {

// Mean-reverting random price walk (Ornstein-Uhlenbeck discretized)
// price += theta * (mean - price) * dt + sigma * sqrt(dt) * N(0,1)
class PriceWalk {
public:
    PriceWalk(double start_price, double volatility, double tick_size, uint32_t seed);

    // Generate next price (fixed-point) using internal RNG
    Price next();

    // Generate next price using externally provided standard normal draw
    // Used by MultiAssetSim to inject correlated noise
    Price next(double noise);

    // Current price as double
    double current_double() const { return current_price_; }

    // Current price as fixed-point
    Price current() const;

    // Adjust volatility (used by VolatilityRegime)
    void set_volatility(double vol) { volatility_ = vol; }
    double volatility() const { return volatility_; }

    // Apply an instant price jump as a fraction of current price (used by NewsEventGenerator)
    void apply_jump(double fraction);

private:
    double current_price_;
    double mean_price_;
    double volatility_;
    double tick_size_;
    double theta_ = 0.1;     // mean-reversion speed
    double dt_    = 0.001;   // time step

    std::mt19937 rng_;
    std::normal_distribution<double> normal_{0.0, 1.0};

    double snap_to_tick(double price) const;
};

}  // namespace qf::simulator
