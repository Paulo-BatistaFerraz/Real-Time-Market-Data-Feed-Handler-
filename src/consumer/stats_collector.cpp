#include "mdfh/consumer/stats_collector.hpp"

namespace mdfh::consumer {

// TODO: Implement stats collection logic

void StatsCollector::process(const core::BookUpdate& update) {
    (void)update;
}

void StatsCollector::tick() {
    throughput_.tick(updates_this_tick_);
    updates_this_tick_ = 0;
}

StatsSnapshot StatsCollector::snapshot() const {
    return {
        histogram_.p50(),
        histogram_.p95(),
        histogram_.p99(),
        histogram_.p999(),
        histogram_.min(),
        histogram_.max(),
        throughput_.current_rate(),
        throughput_.peak_rate(),
        throughput_.total(),
        gap_count_
    };
}

void StatsCollector::reset() {
    histogram_.reset();
    throughput_.reset();
    updates_this_tick_ = 0;
    gap_count_ = 0;
}

}  // namespace mdfh::consumer
