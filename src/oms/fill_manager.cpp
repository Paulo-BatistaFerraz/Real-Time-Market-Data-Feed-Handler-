#include "qf/oms/fill_manager.hpp"
#include "qf/oms/order_manager.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::oms {

const SymbolFillStats FillManager::empty_stats_{};

FillManager::FillManager(OrderManager& order_mgr, strategy::Portfolio& portfolio)
    : order_mgr_(order_mgr), portfolio_(portfolio) {}

OMResult FillManager::on_fill(const FillReport& report) {
    // 1. Forward to OrderManager (validates order exists, updates state).
    OMResult result = order_mgr_.on_fill(report);
    if (!result.success) {
        return result;
    }

    // 2. Retrieve the order to get symbol and side for Portfolio.
    auto order_opt = order_mgr_.get_order(report.order_id);
    if (!order_opt) {
        return {false, report.order_id, "Order not found after fill"};
    }
    const auto& order = *order_opt;

    // 3. Feed Portfolio with the fill details.
    portfolio_.on_fill(report.timestamp, order.symbol, order.side,
                       report.fill_price, report.fill_quantity);

    // 4. Update aggregate statistics.
    ++total_fills_;
    total_volume_ += report.fill_quantity;

    uint64_t key = order.symbol.as_key();
    auto& sym_stats = stats_[key];
    ++sym_stats.fill_count;
    sym_stats.total_volume  += report.fill_quantity;
    sym_stats.total_notional += price_to_double(report.fill_price) * report.fill_quantity;

    return result;
}

const SymbolFillStats& FillManager::stats_for(const Symbol& symbol) const {
    auto it = stats_.find(symbol.as_key());
    return (it != stats_.end()) ? it->second : empty_stats_;
}

}  // namespace qf::oms
