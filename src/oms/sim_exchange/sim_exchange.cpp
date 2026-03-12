#include "qf/oms/sim_exchange/sim_exchange.hpp"
#include "qf/common/clock.hpp"

#include <algorithm>

namespace qf::oms {

SimExchange::SimExchange(FillCallback on_fill, const FillModelConfig& config)
    : on_fill_(std::move(on_fill)), fill_model_(config) {}

void SimExchange::submit_order(const OmsOrder& order) {
    // Random reject check
    if (fill_model_.should_reject()) {
        ++total_rejects_;
        // Generate a reject fill report (zero quantity, is_final = true)
        FillReport reject;
        reject.order_id = order.order_id;
        reject.fill_price = 0;
        reject.fill_quantity = 0;
        reject.timestamp = fill_model_.apply_latency(Clock::now_ns());
        reject.is_final = true;
        on_fill_(reject);
        return;
    }

    if (order.type == OrderType::Market) {
        // Market orders fill immediately at best bid/ask
        Price fill_price = market_price_for(order.symbol, order.side);
        if (fill_price == 0) {
            // No market price available — reject
            ++total_rejects_;
            FillReport reject;
            reject.order_id = order.order_id;
            reject.fill_price = 0;
            reject.fill_quantity = 0;
            reject.timestamp = fill_model_.apply_latency(Clock::now_ns());
            reject.is_final = true;
            on_fill_(reject);
            return;
        }
        generate_fills(order, fill_price);
    } else {
        // Limit orders: try to fill immediately, otherwise queue
        if (!try_fill_limit(order)) {
            pending_orders_.push_back(order);
        }
    }
}

void SimExchange::on_market_update(const Symbol& symbol, Price best_bid, Price best_ask) {
    uint64_t key = symbol.as_key();
    quotes_[key] = {best_bid, best_ask};
    check_pending_orders();
}

void SimExchange::check_pending_orders() {
    // Try to fill pending limit orders. Remove those that fill.
    auto it = std::remove_if(pending_orders_.begin(), pending_orders_.end(),
        [this](const OmsOrder& order) {
            return try_fill_limit(order);
        });
    pending_orders_.erase(it, pending_orders_.end());
}

void SimExchange::generate_fills(const OmsOrder& order, Price fill_price) {
    Timestamp base_ts = Clock::now_ns();
    std::vector<Quantity> partial_quantities = fill_model_.split_quantity(order.remaining());

    for (size_t i = 0; i < partial_quantities.size(); ++i) {
        FillReport report;
        report.order_id = order.order_id;
        report.fill_price = fill_price;
        report.fill_quantity = partial_quantities[i];
        report.timestamp = fill_model_.apply_latency(base_ts + static_cast<uint64_t>(i) * 1000);
        report.is_final = (i == partial_quantities.size() - 1);

        ++total_fills_;
        on_fill_(report);
    }
}

bool SimExchange::try_fill_limit(const OmsOrder& order) {
    Price market = market_price_for(order.symbol, order.side);
    if (market == 0) return false;

    // Buy limit: fill if market ask <= limit price
    // Sell limit: fill if market bid >= limit price
    bool should_fill = false;
    if (order.side == Side::Buy) {
        should_fill = (market <= order.price);
    } else {
        should_fill = (market >= order.price);
    }

    if (should_fill) {
        // Fill at the limit price (price improvement not modeled)
        generate_fills(order, order.price);
        return true;
    }
    return false;
}

Price SimExchange::market_price_for(const Symbol& symbol, Side side) const {
    auto it = quotes_.find(symbol.as_key());
    if (it == quotes_.end()) return 0;

    // Buyers fill at the ask, sellers fill at the bid
    return (side == Side::Buy) ? it->second.best_ask : it->second.best_bid;
}

}  // namespace qf::oms
