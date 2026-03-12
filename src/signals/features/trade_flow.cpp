#include "qf/signals/features/trade_flow.hpp"
#include "qf/common/types.hpp"

namespace qf::signals {

TradeFlow::TradeFlow(size_t window_size)
    : window_size_(window_size) {}

double TradeFlow::compute(const core::OrderBook& book, const TradeHistory& trades) {
    auto bid = book.best_bid();
    auto ask = book.best_ask();

    // Need both sides to compute mid-price for aggressor classification
    double mid = 0.0;
    bool has_mid = false;
    if (bid && ask) {
        mid = (price_to_double(*bid) + price_to_double(*ask)) / 2.0;
        has_mid = true;
    }

    // Ingest new trades since last compute
    const auto& trade_vec = trades.trades();
    for (size_t i = last_processed_; i < trade_vec.size(); ++i) {
        const auto& t = trade_vec[i];
        double trade_price = price_to_double(t.price);

        AggressorSide side;
        if (has_mid) {
            // Classify by comparing trade price to mid:
            // trade at or above mid = buy aggressor (lifting the offer)
            // trade below mid = sell aggressor (hitting the bid)
            side = (trade_price >= mid) ? AggressorSide::Buy : AggressorSide::Sell;
        } else {
            // Fall back to the trade's own aggressor_side field
            side = (t.aggressor_side == Side::Buy) ? AggressorSide::Buy : AggressorSide::Sell;
        }

        add_sample(side);
    }
    last_processed_ = trade_vec.size();

    size_t total = samples_.size();
    if (total == 0) {
        return 0.0;
    }

    return (static_cast<double>(buy_count_) - static_cast<double>(sell_count_))
           / static_cast<double>(total);
}

std::string TradeFlow::name() {
    return "trade_flow";
}

double TradeFlow::buy_ratio() const {
    size_t total = samples_.size();
    if (total == 0) return 0.0;
    return static_cast<double>(buy_count_) / static_cast<double>(total);
}

double TradeFlow::sell_ratio() const {
    size_t total = samples_.size();
    if (total == 0) return 0.0;
    return static_cast<double>(sell_count_) / static_cast<double>(total);
}

void TradeFlow::add_sample(AggressorSide side) {
    // Evict oldest if at capacity
    if (samples_.size() >= window_size_) {
        AggressorSide old = samples_.front();
        samples_.pop_front();
        if (old == AggressorSide::Buy)  --buy_count_;
        else                            --sell_count_;
    }

    samples_.push_back(side);
    if (side == AggressorSide::Buy) ++buy_count_;
    else                            ++sell_count_;
}

}  // namespace qf::signals
