#include "qf/signals/features/twap.hpp"
#include "qf/common/types.hpp"
#include <cmath>

namespace qf::signals {

TWAP::TWAP(uint64_t window_ns)
    : window_ns_(window_ns) {}

double TWAP::compute(const core::OrderBook& book, const TradeHistory& trades) {
    // Get the current mid price from the order book
    auto bid = book.best_bid();
    auto ask = book.best_ask();

    if (!bid && !ask) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double mid_price;
    if (bid && ask) {
        mid_price = (price_to_double(*bid) + price_to_double(*ask)) / 2.0;
    } else if (bid) {
        mid_price = price_to_double(*bid);
    } else {
        mid_price = price_to_double(*ask);
    }

    // Determine timestamp: use latest trade if available, otherwise 0
    uint64_t now = 0;
    if (!trades.empty()) {
        now = trades.latest().timestamp;
    }

    // Add the current sample
    samples_.push_back({mid_price, now});

    // Evict samples outside the window
    evict_old(now);

    if (samples_.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Compute time-weighted average
    // For each consecutive pair of samples, weight the earlier sample's price
    // by the time duration until the next sample.
    if (samples_.size() == 1) {
        return samples_.front().price;
    }

    double weighted_sum = 0.0;
    uint64_t total_time = 0;

    for (size_t i = 0; i + 1 < samples_.size(); ++i) {
        uint64_t dt = samples_[i + 1].timestamp - samples_[i].timestamp;
        weighted_sum += samples_[i].price * static_cast<double>(dt);
        total_time += dt;
    }

    if (total_time == 0) {
        // All samples have the same timestamp — simple average
        double sum = 0.0;
        for (const auto& s : samples_) {
            sum += s.price;
        }
        return sum / static_cast<double>(samples_.size());
    }

    return weighted_sum / static_cast<double>(total_time);
}

std::string TWAP::name() {
    return "twap";
}

void TWAP::evict_old(uint64_t now) {
    if (now < window_ns_) {
        return;  // not enough elapsed time to evict
    }
    uint64_t cutoff = now - window_ns_;
    while (!samples_.empty() && samples_.front().timestamp < cutoff) {
        samples_.pop_front();
    }
}

}  // namespace qf::signals
