#pragma once

#include <cstdint>
#include "qf/core/pipeline_types.hpp"
#include "qf/consumer/latency_histogram.hpp"
#include "qf/consumer/throughput_tracker.hpp"

namespace qf::consumer {

struct StatsSnapshot {
    uint64_t p50_ns;
    uint64_t p95_ns;
    uint64_t p99_ns;
    uint64_t p999_ns;
    uint64_t min_ns;
    uint64_t max_ns;
    double   msg_rate;
    double   peak_rate;
    uint64_t total_updates;
    uint64_t gap_count;
};

class StatsCollector {
public:
    StatsCollector() = default;

    // Process a BookUpdate from the pipeline
    void process(const core::BookUpdate& update);

    // Called once per second to advance throughput window
    void tick();

    // Take a snapshot of current stats
    StatsSnapshot snapshot() const;

    void set_gap_count(uint64_t gaps) { gap_count_ = gaps; }

    void reset();

private:
    LatencyHistogram histogram_;
    ThroughputTracker throughput_;
    uint64_t updates_this_tick_ = 0;
    uint64_t gap_count_ = 0;
};

}  // namespace qf::consumer
