#include "qf/consumer/throughput_tracker.hpp"
#include <numeric>
#include <algorithm>

namespace qf::consumer {

ThroughputTracker::ThroughputTracker(size_t window_seconds)
    : window_(window_seconds, 0), window_size_(window_seconds) {}

void ThroughputTracker::tick(uint64_t count) {
    window_[write_pos_] = count;
    write_pos_ = (write_pos_ + 1) % window_size_;
    if (!filled_ && write_pos_ == 0) filled_ = true;
    total_ += count;

    double rate = current_rate();
    if (rate > peak_rate_) peak_rate_ = rate;
}

double ThroughputTracker::current_rate() const {
    size_t slots = filled_ ? window_size_ : write_pos_;
    if (slots == 0) return 0.0;

    uint64_t sum = filled_
        ? std::accumulate(window_.begin(), window_.end(), uint64_t(0))
        : std::accumulate(window_.begin(), window_.begin() + write_pos_, uint64_t(0));
    return static_cast<double>(sum) / static_cast<double>(slots);
}

void ThroughputTracker::reset() {
    std::fill(window_.begin(), window_.end(), 0);
    write_pos_ = 0;
    filled_ = false;
    total_ = 0;
    peak_rate_ = 0.0;
}

}  // namespace qf::consumer
