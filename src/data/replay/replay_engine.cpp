#include "qf/data/replay/replay_engine.hpp"

#include <algorithm>
#include <cstring>

namespace qf::data {

ReplayEngine::ReplayEngine(TickStore& store, ReplayClock& clock, ReplayCallback callback)
    : store_(store), clock_(clock), callback_(std::move(callback)) {}

void ReplayEngine::load(Timestamp start_time, Timestamp end_time) {
    ticks_ = store_.read_range(start_time, end_time);
    // Ensure ticks are sorted by timestamp for correct replay order.
    std::sort(ticks_.begin(), ticks_.end(),
              [](const TickRecord& a, const TickRecord& b) {
                  return a.timestamp < b.timestamp;
              });
}

void ReplayEngine::load(const Symbol& symbol, Timestamp start_time, Timestamp end_time) {
    ticks_ = store_.read_range(symbol, start_time, end_time);
    std::sort(ticks_.begin(), ticks_.end(),
              [](const TickRecord& a, const TickRecord& b) {
                  return a.timestamp < b.timestamp;
              });
}

void ReplayEngine::start() {
    if (ticks_.empty()) return;

    running_.store(true, std::memory_order_release);
    ticks_published_.store(0, std::memory_order_relaxed);
    bytes_published_.store(0, std::memory_order_relaxed);

    // Start the clock at the first tick's timestamp.
    clock_.start(ticks_.front().timestamp);

    for (const auto& tick : ticks_) {
        if (!running_.load(std::memory_order_acquire)) break;

        // Wait until virtual time reaches this tick's timestamp.
        clock_.wait_until(tick.timestamp);
        if (!running_.load(std::memory_order_acquire)) break;

        // In as-fast-as-possible mode, advance the clock.
        if (clock_.speed() == 0.0) {
            clock_.advance_to(tick.timestamp);
        }

        // Publish: deliver the raw payload to the callback.
        callback_(tick.payload, tick.payload_length, tick.timestamp);

        ticks_published_.fetch_add(1, std::memory_order_relaxed);
        bytes_published_.fetch_add(tick.payload_length, std::memory_order_relaxed);
    }

    running_.store(false, std::memory_order_release);
}

void ReplayEngine::stop() {
    running_.store(false, std::memory_order_release);
}

}  // namespace qf::data
