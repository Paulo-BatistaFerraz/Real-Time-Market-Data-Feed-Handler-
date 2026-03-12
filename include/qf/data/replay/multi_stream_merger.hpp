#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/data/data_types.hpp"
#include "qf/data/storage/tick_store.hpp"

namespace qf::data {

// Callback for merged tick events: (tick_record)
using MergedTickCallback = std::function<void(const TickRecord&)>;

// MultiStreamMerger — merges tick streams from multiple symbols by timestamp.
// Uses a min-heap (priority queue) to emit events in global time order.
class MultiStreamMerger {
public:
    MultiStreamMerger();

    // Add a stream of ticks for a symbol from a TickStore over [start, end].
    void add_stream(TickStore& store, const Symbol& symbol,
                    Timestamp start_time, Timestamp end_time);

    // Add raw ticks directly (e.g., already loaded from multiple sources).
    void add_ticks(const std::vector<TickRecord>& ticks);

    // Merge all added streams and invoke the callback in global time order.
    // Blocks until all ticks are emitted or stop() is called.
    void merge(MergedTickCallback callback);

    // Signal the merger to stop mid-merge.
    void stop();

    // Whether a merge is currently running.
    bool running() const { return running_.load(std::memory_order_acquire); }

    // Total ticks merged so far in the current/last merge.
    uint64_t ticks_merged() const { return ticks_merged_.load(std::memory_order_relaxed); }

    // Total number of ticks loaded across all streams.
    size_t total_ticks() const { return total_ticks_; }

    // Number of distinct streams added.
    size_t stream_count() const { return stream_count_; }

private:
    // Entry in the min-heap: index into the flat ticks vector + stream id.
    struct HeapEntry {
        Timestamp timestamp;
        size_t    tick_index;  // index into merged_ticks_

        // Min-heap: smallest timestamp has highest priority.
        bool operator>(const HeapEntry& other) const {
            return timestamp > other.timestamp;
        }
    };

    using MinHeap = std::priority_queue<HeapEntry, std::vector<HeapEntry>,
                                        std::greater<HeapEntry>>;

    std::vector<TickRecord> merged_ticks_;   // all loaded ticks (unsorted)
    size_t                  total_ticks_{0};
    size_t                  stream_count_{0};
    std::atomic<bool>       running_{false};
    std::atomic<uint64_t>   ticks_merged_{0};
};

}  // namespace qf::data
