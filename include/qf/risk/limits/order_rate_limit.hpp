#pragma once

#include <cstdint>
#include <deque>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::risk {

// OrderRateLimit — enforces a maximum number of orders per second.
//
// Uses a sliding window counter: timestamps of recent orders are stored
// in a deque, and expired entries (older than 1 second) are pruned on
// each check.  The check fails if the count of orders within the window
// already meets or exceeds the configured maximum.
class OrderRateLimit {
public:
    // Construct with the maximum number of orders allowed per second.
    explicit OrderRateLimit(uint32_t max_orders_per_second);

    // Pre-trade check: would sending another order breach the rate limit?
    // |current_ts| is the current timestamp in nanoseconds.
    RiskResult check(Timestamp current_ts) const;

    // Record that an order was sent at the given timestamp.
    void record_order(Timestamp ts);

    // Reset all recorded timestamps.
    void reset();

    // Accessors.
    uint32_t max_orders_per_second() const { return max_orders_per_second_; }
    size_t   current_window_count() const { return timestamps_.size(); }

private:
    uint32_t max_orders_per_second_;
    mutable std::deque<Timestamp> timestamps_;

    static constexpr uint64_t ONE_SECOND_NS = 1'000'000'000ULL;

    void prune(Timestamp current_ts) const;
};

}  // namespace qf::risk
