#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "qf/common/constants.hpp"

namespace qf::consumer {

class LatencyHistogram {
public:
    explicit LatencyHistogram(uint64_t max_latency_us = HISTOGRAM_MAX_US,
                              size_t bucket_count = HISTOGRAM_BUCKETS);

    // Record a latency sample (in nanoseconds)
    void record(uint64_t latency_ns);

    // Get percentile value in nanoseconds (p = 0.0 to 1.0)
    uint64_t percentile(double p) const;

    // Convenience
    uint64_t p50()   const { return percentile(0.50); }
    uint64_t p95()   const { return percentile(0.95); }
    uint64_t p99()   const { return percentile(0.99); }
    uint64_t p999()  const { return percentile(0.999); }

    uint64_t min() const { return min_; }
    uint64_t max() const { return max_; }
    double   mean() const;
    uint64_t count() const { return count_; }

    void reset();

private:
    std::vector<uint64_t> buckets_;
    uint64_t max_latency_ns_;
    uint64_t bucket_width_ns_;
    uint64_t count_ = 0;
    uint64_t sum_   = 0;
    uint64_t min_   = UINT64_MAX;
    uint64_t max_   = 0;
    uint64_t overflow_ = 0;

    size_t bucket_index(uint64_t latency_ns) const;
};

}  // namespace qf::consumer
