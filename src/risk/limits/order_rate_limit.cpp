#include "qf/risk/limits/order_rate_limit.hpp"

#include <sstream>

namespace qf::risk {

OrderRateLimit::OrderRateLimit(uint32_t max_orders_per_second)
    : max_orders_per_second_(max_orders_per_second) {}

RiskResult OrderRateLimit::check(Timestamp current_ts) const {
    prune(current_ts);

    const auto count = static_cast<uint32_t>(timestamps_.size());
    const bool passed = count < max_orders_per_second_;

    std::ostringstream msg;
    if (!passed) {
        msg << "Order rate limit breached: " << count
            << " orders in last second >= " << max_orders_per_second_;
    }

    return RiskResult{
        RiskCheck::OrderRateLimit,
        passed,
        static_cast<double>(count),
        static_cast<double>(max_orders_per_second_),
        msg.str()
    };
}

void OrderRateLimit::record_order(Timestamp ts) {
    timestamps_.push_back(ts);
}

void OrderRateLimit::reset() {
    timestamps_.clear();
}

void OrderRateLimit::prune(Timestamp current_ts) const {
    while (!timestamps_.empty() &&
           timestamps_.front() + ONE_SECOND_NS <= current_ts) {
        timestamps_.pop_front();
    }
}

}  // namespace qf::risk
