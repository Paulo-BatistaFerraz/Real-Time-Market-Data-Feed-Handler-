#pragma once

#include "qf/oms/oms_types.hpp"
#include "qf/oms/sim_exchange/sim_fill_model.hpp"
#include "qf/core/order_book.hpp"

#include <functional>
#include <unordered_map>
#include <vector>

namespace qf::oms {

// Forward declaration — FillManager receives generated fills.
class FillManager;

// SimExchange accepts orders and generates fills based on current market prices.
// Market orders: fill immediately at best bid/ask.
// Limit orders: fill when market price reaches limit price.
class SimExchange {
public:
    using FillCallback = std::function<void(const FillReport&)>;

    explicit SimExchange(FillCallback on_fill, const FillModelConfig& config = {});

    // Submit an order to the exchange.
    // Market orders are filled immediately; limit orders are queued.
    void submit_order(const OmsOrder& order);

    // Update market prices for a symbol. Triggers pending limit order checks.
    void on_market_update(const Symbol& symbol, Price best_bid, Price best_ask);

    // Process all pending limit orders against current prices.
    // Called periodically or after market updates.
    void check_pending_orders();

    // Statistics
    size_t pending_order_count() const { return pending_orders_.size(); }
    uint64_t total_fills() const { return total_fills_; }
    uint64_t total_rejects() const { return total_rejects_; }

    SimFillModel& fill_model() { return fill_model_; }

private:
    FillCallback on_fill_;
    SimFillModel fill_model_;

    // Pending limit orders waiting for price to reach their limit.
    std::vector<OmsOrder> pending_orders_;

    // Last known best bid/ask per symbol.
    struct MarketQuote {
        Price best_bid{0};
        Price best_ask{0};
    };
    std::unordered_map<uint64_t, MarketQuote> quotes_;

    uint64_t total_fills_{0};
    uint64_t total_rejects_{0};

    // Generate fill reports for an order at the given price.
    void generate_fills(const OmsOrder& order, Price fill_price);

    // Try to fill a limit order against current market. Returns true if filled.
    bool try_fill_limit(const OmsOrder& order);

    // Get current market price for a side (buy fills at ask, sell fills at bid).
    Price market_price_for(const Symbol& symbol, Side side) const;
};

}  // namespace qf::oms
