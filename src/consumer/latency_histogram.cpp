#include "qf/consumer/latency_histogram.hpp"
#include <algorithm>

namespace qf::consumer {

LatencyHistogram::LatencyHistogram(uint64_t max_latency_us, size_t bucket_count)
    : buckets_(bucket_count, 0)
    , max_latency_ns_(max_latency_us * 1000)
    , bucket_width_ns_(max_latency_ns_ / bucket_count) {}

void LatencyHistogram::record(uint64_t latency_ns) {
    size_t idx = bucket_index(latency_ns);
    ++buckets_[idx];
    ++count_;
    sum_ += latency_ns;
    if (latency_ns < min_) min_ = latency_ns;
    if (latency_ns > max_) max_ = latency_ns;
    if (latency_ns >= max_latency_ns_) ++overflow_;
}

uint64_t LatencyHistogram::percentile(double p) const {
    if (count_ == 0) return 0;

    uint64_t threshold = static_cast<uint64_t>(p * static_cast<double>(count_));
    if (threshold == 0) threshold = 1;

    uint64_t cumulative = 0;
    for (size_t i = 0; i < buckets_.size(); ++i) {
        cumulative += buckets_[i];
        if (cumulative >= threshold) {
            return (i + 1) * bucket_width_ns_;
        }
    }
    return max_latency_ns_;
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

}  // namespace qf::consumer
