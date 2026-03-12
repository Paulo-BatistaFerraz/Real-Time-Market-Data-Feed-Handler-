#include "qf/signals/features/microprice.hpp"
#include "qf/common/types.hpp"
#include <cmath>

namespace qf::signals {

double Microprice::compute(const core::OrderBook& book, const TradeHistory& /*trades*/) {
    auto bid = book.best_bid();
    auto ask = book.best_ask();

    // Return NaN if no bid or ask available
    if (!bid || !ask) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double bid_price = price_to_double(*bid);
    double ask_price = price_to_double(*ask);
    double bid_qty   = static_cast<double>(book.best_bid_qty());
    double ask_qty   = static_cast<double>(book.best_ask_qty());

    double total_qty = bid_qty + ask_qty;
    if (total_qty == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Microprice: size-weighted mid
    // (bid_price * ask_qty + ask_price * bid_qty) / (bid_qty + ask_qty)
    return (bid_price * ask_qty + ask_price * bid_qty) / total_qty;
}

std::string Microprice::name() {
    return "microprice";
}

}  // namespace qf::signals
