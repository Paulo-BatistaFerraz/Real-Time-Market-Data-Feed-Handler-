#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace mdfh::consumer {

class ThroughputTracker {
public:
    explicit ThroughputTracker(size_t window_seconds = 5);

    // Record N items processed in the current tick
    void tick(uint64_t count);

    // Current rate (items/sec averaged over window)
    double current_rate() const;

    // Peak rate seen
    double peak_rate() const { return peak_rate_; }

    // Total items ever recorded
    uint64_t total() const { return total_; }

    void reset();

private:
    std::vector<uint64_t> window_;
    size_t window_size_;
    size_t write_pos_ = 0;
    bool   filled_    = false;
    uint64_t total_   = 0;
    double peak_rate_ = 0.0;
};

}  // namespace mdfh::consumer
