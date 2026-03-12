#include "qf/simulator/price_walk.hpp"
#include <cmath>
#include <algorithm>

namespace qf::simulator {

PriceWalk::PriceWalk(double start_price, double volatility, double tick_size, uint32_t seed)
    : current_price_(start_price)
    , mean_price_(start_price)
    , volatility_(volatility)
    , tick_size_(tick_size)
    , rng_(seed) {}

Price PriceWalk::next() {
    return next(normal_(rng_));
}

Price PriceWalk::next(double noise) {
    // Ornstein-Uhlenbeck discretized:
    // price += theta * (mean - price) * dt + sigma * sqrt(dt) * noise
    double drift = theta_ * (mean_price_ - current_price_) * dt_;
    double diffusion = volatility_ * std::sqrt(dt_) * noise;
    double new_price = current_price_ + drift + diffusion;

    // Clamp: no negative prices, no more than 2x the mean
    double min_price = tick_size_;
    double max_price = mean_price_ * 2.0;
    new_price = std::clamp(new_price, min_price, max_price);

    // Snap to tick grid
    current_price_ = snap_to_tick(new_price);
    return current();
}

Price PriceWalk::current() const {
    return double_to_price(current_price_);
}

void PriceWalk::apply_jump(double fraction) {
    double jump = current_price_ * fraction;
    double new_price = current_price_ + jump;

    double min_price = tick_size_;
    double max_price = mean_price_ * 2.0;
    new_price = std::clamp(new_price, min_price, max_price);

    current_price_ = snap_to_tick(new_price);
}

double PriceWalk::snap_to_tick(double price) const {
    return std::round(price / tick_size_) * tick_size_;
}

}  // namespace qf::simulator
