#include "qf/signals/features/vwap.hpp"
#include "qf/common/types.hpp"

namespace qf::signals {

double VWAP::compute(const core::OrderBook& /*book*/, const TradeHistory& trades) {
    // Process any new trades since last compute
    const auto& trade_vec = trades.trades();
    for (size_t i = last_processed_; i < trade_vec.size(); ++i) {
        double price = price_to_double(trade_vec[i].price);
        double qty   = static_cast<double>(trade_vec[i].quantity);
        cum_price_qty_ += price * qty;
        cum_qty_       += qty;
    }
    last_processed_ = trade_vec.size();

    if (cum_qty_ == 0.0) {
        return 0.0;
    }
    return cum_price_qty_ / cum_qty_;
}

std::string VWAP::name() {
    return "vwap";
}

void VWAP::reset() {
    cum_price_qty_  = 0.0;
    cum_qty_        = 0.0;
    last_processed_ = 0;
}

}  // namespace qf::signals
