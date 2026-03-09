#include "mdfh/simulator/price_walk.hpp"
#include <cmath>
#include <algorithm>

namespace mdfh::simulator {

// TODO: Implement Ornstein-Uhlenbeck price walk

PriceWalk::PriceWalk(double start_price, double volatility, double tick_size, uint32_t seed)
    : current_price_(start_price)
    , mean_price_(start_price)
    , volatility_(volatility)
    , tick_size_(tick_size)
    , rng_(seed) {}

Price PriceWalk::next() {
    // price += theta * (mean - price) * dt + sigma * sqrt(dt) * N(0,1)
    (void)theta_; (void)dt_;
    return current();
}

Price PriceWalk::current() const {
    return double_to_price(current_price_);
}

double PriceWalk::snap_to_tick(double price) const {
    return std::round(price / tick_size_) * tick_size_;
}

}  // namespace mdfh::simulator
