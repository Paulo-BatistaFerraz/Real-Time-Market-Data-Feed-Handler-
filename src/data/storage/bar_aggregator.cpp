#include "qf/data/storage/bar_aggregator.hpp"

namespace qf::data {

BarAggregator::BarAggregator(BarInterval interval, BarCallback on_bar)
    : interval_(interval), on_bar_(std::move(on_bar)) {}

void BarAggregator::add_tick(const Symbol& symbol, Price price,
                             Quantity quantity, Timestamp timestamp) {
    uint64_t key = symbol.as_key();
    auto& state = bars_[key];

    if (!state.active) {
        // Start a new bar.
        state.bar = Bar();
        state.bar.symbol     = symbol;
        state.bar.open       = price;
        state.bar.high       = price;
        state.bar.low        = price;
        state.bar.close      = price;
        state.bar.volume     = quantity;
        state.bar.tick_count = 1;
        state.bar.start_time = align_to_interval(timestamp);
        state.bar.end_time   = timestamp;
        state.active = true;
        return;
    }

    // Check if this tick belongs to a new interval.
    Timestamp bar_start = align_to_interval(timestamp);
    if (bar_start != state.bar.start_time) {
        // The current bar is complete — emit it.
        emit_bar(state.bar);

        // Start a new bar with this tick.
        state.bar = Bar();
        state.bar.symbol     = symbol;
        state.bar.open       = price;
        state.bar.high       = price;
        state.bar.low        = price;
        state.bar.close      = price;
        state.bar.volume     = quantity;
        state.bar.tick_count = 1;
        state.bar.start_time = bar_start;
        state.bar.end_time   = timestamp;
        return;
    }

    // Same interval — update OHLCV.
    if (price > state.bar.high) state.bar.high = price;
    if (price < state.bar.low)  state.bar.low  = price;
    state.bar.close = price;
    state.bar.volume += quantity;
    state.bar.tick_count++;
    state.bar.end_time = timestamp;
}

void BarAggregator::flush() {
    for (auto& [key, state] : bars_) {
        if (state.active) {
            emit_bar(state.bar);
            state.active = false;
        }
    }
}

const Bar* BarAggregator::current_bar(const Symbol& symbol) const {
    auto it = bars_.find(symbol.as_key());
    if (it != bars_.end() && it->second.active) {
        return &it->second.bar;
    }
    return nullptr;
}

Timestamp BarAggregator::align_to_interval(Timestamp ts) const {
    uint64_t ns = static_cast<uint64_t>(interval_);
    return (ts / ns) * ns;
}

void BarAggregator::emit_bar(Bar& bar) {
    if (on_bar_) {
        on_bar_(bar);
    }
}

}  // namespace qf::data
