#include "qf/data/replay/multi_stream_merger.hpp"

#include <algorithm>

namespace qf::data {

MultiStreamMerger::MultiStreamMerger() = default;

void MultiStreamMerger::add_stream(TickStore& store, const Symbol& symbol,
                                   Timestamp start_time, Timestamp end_time) {
    auto ticks = store.read_range(symbol, start_time, end_time);
    merged_ticks_.insert(merged_ticks_.end(), ticks.begin(), ticks.end());
    total_ticks_ = merged_ticks_.size();
    ++stream_count_;
}

void MultiStreamMerger::add_ticks(const std::vector<TickRecord>& ticks) {
    merged_ticks_.insert(merged_ticks_.end(), ticks.begin(), ticks.end());
    total_ticks_ = merged_ticks_.size();
    ++stream_count_;
}

void MultiStreamMerger::merge(MergedTickCallback callback) {
    if (merged_ticks_.empty()) return;

    running_.store(true, std::memory_order_release);
    ticks_merged_.store(0, std::memory_order_relaxed);

    // Build the min-heap from all loaded ticks.
    MinHeap heap;
    for (size_t i = 0; i < merged_ticks_.size(); ++i) {
        heap.push(HeapEntry{merged_ticks_[i].timestamp, i});
    }

    // Emit events in global time order.
    while (!heap.empty()) {
        if (!running_.load(std::memory_order_acquire)) break;

        auto entry = heap.top();
        heap.pop();

        callback(merged_ticks_[entry.tick_index]);
        ticks_merged_.fetch_add(1, std::memory_order_relaxed);
    }

    running_.store(false, std::memory_order_release);
}

void MultiStreamMerger::stop() {
    running_.store(false, std::memory_order_release);
}

}  // namespace qf::data
