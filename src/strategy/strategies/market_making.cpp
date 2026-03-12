#include "qf/strategy/strategies/market_making.hpp"

#include <algorithm>
#include <cmath>

namespace qf::strategy {

MarketMakingStrategy::MarketMakingStrategy() = default;

std::string MarketMakingStrategy::name() {
    return "market_making";
}

void MarketMakingStrategy::configure(const Config& config) {
    spread_ticks_ = config.get<uint32_t>("strategy.market_making.spread_ticks", 2u);
    quote_size_ = config.get<uint32_t>("strategy.market_making.quote_size", 100u);
    max_position_ = static_cast<int64_t>(
        config.get<uint32_t>("strategy.market_making.max_position", 1000u));
    inventory_skew_factor_ = config.get<double>(
        "strategy.market_making.inventory_skew_factor", 0.5);
}

std::vector<Decision> MarketMakingStrategy::on_signal(
    const signals::Signal& signal,
    const PortfolioView& portfolio) {

    std::vector<Decision> decisions;

    // Derive fair value from signal.
    Price fair_value = fair_value_from_signal(signal);
    if (fair_value == 0) {
        return decisions;
    }

    // Check if fair value moved — cancel and replace (always re-quote).
    // In a real system this would send explicit cancel messages; here the
    // engine treats each on_signal call as a full quote refresh.
    last_fair_value_ = fair_value;

    // Current inventory for this symbol.
    std::string sym(signal.symbol.data, strnlen(signal.symbol.data, SYMBOL_LENGTH));
    int64_t position = portfolio.position(sym);

    // Inventory skew: shift quotes to reduce exposure.
    int64_t skew = compute_skew(position);

    // Compute bid and ask prices (symmetric around fair value + skew).
    int64_t half_spread = static_cast<int64_t>(spread_ticks_);
    int64_t bid_raw = static_cast<int64_t>(fair_value) - half_spread + skew;
    int64_t ask_raw = static_cast<int64_t>(fair_value) + half_spread + skew;

    // Clamp to valid price range.
    Price bid_price = static_cast<Price>(std::max<int64_t>(bid_raw, 1));
    Price ask_price = static_cast<Price>(std::max<int64_t>(ask_raw, 1));

    // Only quote the bid side if adding to long wouldn't breach max position.
    bool can_buy = position + static_cast<int64_t>(quote_size_) <= max_position_;
    // Only quote the ask side if going shorter wouldn't breach max position.
    bool can_sell = position - static_cast<int64_t>(quote_size_) >= -max_position_;

    if (can_buy) {
        Decision bid_decision;
        bid_decision.symbol = signal.symbol;
        bid_decision.side = Side::Buy;
        bid_decision.quantity = quote_size_;
        bid_decision.order_type = OrderType::Limit;
        bid_decision.limit_price = bid_price;
        bid_decision.urgency = Urgency::Low;
        decisions.push_back(bid_decision);
    }

    if (can_sell) {
        Decision ask_decision;
        ask_decision.symbol = signal.symbol;
        ask_decision.side = Side::Sell;
        ask_decision.quantity = quote_size_;
        ask_decision.order_type = OrderType::Limit;
        ask_decision.limit_price = ask_price;
        ask_decision.urgency = Urgency::Low;
        decisions.push_back(ask_decision);
    }

    return decisions;
}

int64_t MarketMakingStrategy::compute_skew(int64_t position) const {
    // Negative skew when long (lower prices to sell), positive when short.
    return static_cast<int64_t>(-position * inventory_skew_factor_);
}

Price MarketMakingStrategy::fair_value_from_signal(
    const signals::Signal& signal) const {
    // The signal strength for a market-making strategy is interpreted as the
    // microprice estimate encoded via the composite/ML pipeline.  The
    // timestamp field carries the raw fair-value price when the signal engine
    // emits it; however, when that isn't available we fall back to treating
    // strength as a scaling factor around a baseline.
    //
    // Convention: signal.timestamp carries the most recent microprice in
    // fixed-point form (same encoding as Price).  If zero, the signal is not
    // actionable.
    if (signal.timestamp == 0) {
        return 0;
    }
    return static_cast<Price>(signal.timestamp);
}

}  // namespace qf::strategy
