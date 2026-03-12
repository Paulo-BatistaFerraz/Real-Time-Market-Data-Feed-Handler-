#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include "qf/common/types.hpp"
#include "qf/data/data_types.hpp"

namespace qf::data {

// Supported bar intervals in nanoseconds.
enum class BarInterval : uint64_t {
    OneSecond  = 1'000'000'000ULL,
    OneMinute  = 60'000'000'000ULL,
    FiveMinute = 300'000'000'000ULL,
    OneHour    = 3'600'000'000'000ULL
};

// Callback invoked when a bar is completed (interval boundary crossed).
using BarCallback = std::function<void(const Bar&)>;

// BarAggregator — converts ticks into OHLCV bars at configurable intervals.
// Each symbol maintains its own independent bar state.
class BarAggregator {
public:
    // Construct with a bar interval and an optional completion callback.
    explicit BarAggregator(BarInterval interval, BarCallback on_bar = nullptr);

    // Add a tick. If the tick crosses an interval boundary, the current bar
    // is emitted via the callback and a new bar is started.
    void add_tick(const Symbol& symbol, Price price, Quantity quantity, Timestamp timestamp);

    // Flush all open (incomplete) bars, emitting them via the callback.
    void flush();

    // Set / replace the bar-completion callback.
    void set_callback(BarCallback cb) { on_bar_ = std::move(cb); }

    // Get the interval in nanoseconds.
    uint64_t interval_ns() const { return static_cast<uint64_t>(interval_); }

    // Get the current (incomplete) bar for a symbol, or nullptr if none.
    const Bar* current_bar(const Symbol& symbol) const;

private:
    // Compute the bar-boundary-aligned start time for a given timestamp.
    Timestamp align_to_interval(Timestamp ts) const;

    // Emit a completed bar and reset the slot.
    void emit_bar(Bar& bar);

    BarInterval interval_;
    BarCallback on_bar_;

    // Per-symbol in-progress bar.  Key = Symbol::as_key().
    struct BarState {
        Bar  bar;
        bool active{false};
    };
    std::unordered_map<uint64_t, BarState> bars_;
};

}  // namespace qf::data
