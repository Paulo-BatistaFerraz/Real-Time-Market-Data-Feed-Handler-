#include "qf/signals/features/order_flow_imbalance.hpp"
#include "qf/common/types.hpp"

namespace qf::signals {

OrderFlowImbalance::OrderFlowImbalance(uint64_t window_ns)
    : window_ns_(window_ns) {}

double OrderFlowImbalance::compute(const core::OrderBook& /*book*/, const TradeHistory& trades) {
    // Ingest new trades since last compute
    const auto& trade_vec = trades.trades();
    for (size_t i = last_processed_; i < trade_vec.size(); ++i) {
        const auto& t = trade_vec[i];
        samples_.push_back({
            static_cast<double>(t.quantity),
            t.aggressor_side,
            t.timestamp
        });
    }
    last_processed_ = trade_vec.size();

    if (samples_.empty()) {
        return 0.0;
    }

    // Evict samples outside the rolling window
    evict_old(samples_.back().timestamp);

    // Sum buy and sell volumes within window
    double buy_volume  = 0.0;
    double sell_volume = 0.0;
    for (const auto& s : samples_) {
        if (s.side == Side::Buy) {
            buy_volume += s.quantity;
        } else {
            sell_volume += s.quantity;
        }
    }

    double total = buy_volume + sell_volume;
    if (total == 0.0) {
        return 0.0;
    }

    return (buy_volume - sell_volume) / total;
}

std::string OrderFlowImbalance::name() {
    return "ofi";
}

void OrderFlowImbalance::evict_old(uint64_t now) {
    uint64_t cutoff = (now > window_ns_) ? (now - window_ns_) : 0;
    while (!samples_.empty() && samples_.front().timestamp < cutoff) {
        samples_.pop_front();
    }
}

}  // namespace qf::signals
