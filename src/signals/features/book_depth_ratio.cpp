#include "qf/signals/features/book_depth_ratio.hpp"
#include "qf/common/types.hpp"
#include <cmath>

namespace qf::signals {

BookDepthRatio::BookDepthRatio(size_t num_levels)
    : num_levels_(num_levels) {}

double BookDepthRatio::compute(const core::OrderBook& book, const TradeHistory& /*trades*/) {
    auto bids = book.bid_depth(num_levels_);
    auto asks = book.ask_depth(num_levels_);

    double total_bid = 0.0;
    for (const auto& lvl : bids) {
        total_bid += static_cast<double>(lvl.quantity);
    }

    double total_ask = 0.0;
    for (const auto& lvl : asks) {
        total_ask += static_cast<double>(lvl.quantity);
    }

    // Both sides empty
    if (total_bid == 0.0 && total_ask == 0.0) {
        return 0.0;
    }

    // Ask side empty but bids present — infinite ratio, return NaN
    if (total_ask == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return total_bid / total_ask;
}

std::string BookDepthRatio::name() {
    return "book_depth_ratio";
}

}  // namespace qf::signals
