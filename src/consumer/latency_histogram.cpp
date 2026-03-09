#include "mdfh/consumer/latency_histogram.hpp"
#include <algorithm>

namespace mdfh::consumer {

// TODO: Implement histogram logic

LatencyHistogram::LatencyHistogram(uint64_t max_latency_us, size_t bucket_count)
    : buckets_(bucket_count, 0)
    , max_latency_ns_(max_latency_us * 1000)
    , bucket_width_ns_(max_latency_ns_ / bucket_count) {}

void LatencyHistogram::record(uint64_t latency_ns) {
    (void)latency_ns;
}

uint64_t LatencyHistogram::percentile(double p) const {
    (void)p;
    return 0;
}

double LatencyHistogram::mean() const {
    if (count_ == 0) return 0.0;
    return static_cast<double>(sum_) / static_cast<double>(count_);
}

void LatencyHistogram::reset() {
    std::fill(buckets_.begin(), buckets_.end(), 0);
    count_ = 0;
    sum_ = 0;
    min_ = UINT64_MAX;
    max_ = 0;
    overflow_ = 0;
}

size_t LatencyHistogram::bucket_index(uint64_t latency_ns) const {
    if (bucket_width_ns_ == 0) return 0;
    size_t idx = latency_ns / bucket_width_ns_;
    return std::min(idx, buckets_.size() - 1);
}

}  // namespace mdfh::consumer
