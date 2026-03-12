#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/data/data_types.hpp"
#include "qf/data/replay/replay_clock.hpp"
#include "qf/data/storage/tick_store.hpp"

namespace qf::data {

// Callback matching MulticastReceiver: (data, length, receive_timestamp_ns)
using ReplayCallback = std::function<void(const uint8_t*, size_t, uint64_t)>;

// ReplayEngine — reads tick data from a TickStore and publishes events at
// correct virtual timestamps using a ReplayClock.  Drop-in replacement for
// MulticastReceiver in the pipeline's network stage.
class ReplayEngine {
public:
    ReplayEngine(TickStore& store, ReplayClock& clock, ReplayCallback callback);

    // Load ticks for [start_time, end_time] from the store.
    // Must be called before start().
    void load(Timestamp start_time, Timestamp end_time);

    // Load ticks for a specific symbol within [start_time, end_time].
    void load(const Symbol& symbol, Timestamp start_time, Timestamp end_time);

    // Start replaying loaded ticks.  Blocks the calling thread until all ticks
    // are published or stop() is called.
    void start();

    // Signal the engine to stop.
    void stop();

    // Whether the engine is currently running.
    bool running() const { return running_.load(std::memory_order_acquire); }

    // Stats.
    uint64_t ticks_published() const { return ticks_published_.load(std::memory_order_relaxed); }
    uint64_t bytes_published() const { return bytes_published_.load(std::memory_order_relaxed); }

private:
    TickStore&       store_;
    ReplayClock&     clock_;
    ReplayCallback   callback_;

    std::vector<TickRecord> ticks_;
    std::atomic<bool>       running_{false};
    std::atomic<uint64_t>   ticks_published_{0};
    std::atomic<uint64_t>   bytes_published_{0};
};

}  // namespace qf::data
