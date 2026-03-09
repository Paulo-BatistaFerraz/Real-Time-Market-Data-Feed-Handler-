#pragma once

#include <cstdint>
#include <random>
#include "mdfh/common/types.hpp"

namespace mdfh::simulator {

// Mean-reverting random price walk (Ornstein-Uhlenbeck discretized)
// price += theta * (mean - price) * dt + sigma * sqrt(dt) * N(0,1)
class PriceWalk {
public:
    PriceWalk(double start_price, double volatility, double tick_size, uint32_t seed);

    // Generate next price (fixed-point)
    Price next();

    // Current price as double
    double current_double() const { return current_price_; }

    // Current price as fixed-point
    Price current() const;

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

}  // namespace mdfh::simulator
