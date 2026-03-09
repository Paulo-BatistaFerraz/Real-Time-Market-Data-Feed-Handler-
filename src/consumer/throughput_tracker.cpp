#include "mdfh/consumer/throughput_tracker.hpp"
#include <numeric>
#include <algorithm>

namespace mdfh::consumer {

// TODO: Implement throughput tracking logic

ThroughputTracker::ThroughputTracker(size_t window_seconds)
    : window_(window_seconds, 0), window_size_(window_seconds) {}

void ThroughputTracker::tick(uint64_t count) {
    (void)count;
}

double ThroughputTracker::current_rate() const {
    return 0.0;
}

void ThroughputTracker::reset() {
    std::fill(window_.begin(), window_.end(), 0);
    write_pos_ = 0;
    filled_ = false;
    total_ = 0;
    peak_rate_ = 0.0;
}

}  // namespace mdfh::consumer
