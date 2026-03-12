#pragma once

#include "qf/signals/features/feature_base.hpp"
#include <cstddef>
#include <cstdint>

namespace qf::signals {

// Tick Intensity feature.
// Measures message arrival rate relative to a rolling EWMA baseline.
// compute() returns the ratio of the current tick rate to the average rate:
//   >1 = higher than usual activity (potential unusual activity)
//   <1 = lower than usual activity
//   ~1 = normal activity
// Spike detection: spike_magnitude() returns how many standard deviations
// the current rate deviates from the baseline.
class TickIntensity : public FeatureBase {
public:
    // alpha: smoothing factor for EWMA (higher = more responsive, range (0,1))
    // window_ns: time window in nanoseconds over which to count ticks for rate
    explicit TickIntensity(double alpha = 0.05,
                           uint64_t window_ns = 1'000'000'000ULL);  // 1 second

    double compute(const core::OrderBook& book, const TradeHistory& trades) override;
    std::string name() override;

    // Current instantaneous tick rate (ticks per window)
    double current_rate() const { return current_rate_; }

    // EWMA baseline rate
    double baseline_rate() const { return baseline_rate_; }

    // EWMA baseline variance (for spike detection)
    double baseline_variance() const { return baseline_variance_; }

    // Magnitude of deviation from baseline in standard deviations
    double spike_magnitude() const { return spike_magnitude_; }

    // Number of windows observed so far
    size_t window_count() const { return window_count_; }

private:
    double   alpha_;
    uint64_t window_ns_;

    // EWMA state
    double baseline_rate_     = 0.0;
    double baseline_variance_ = 0.0;
    bool   baseline_initialized_ = false;

    // Current window state
    double   current_rate_    = 0.0;
    double   spike_magnitude_ = 0.0;
    size_t   window_count_    = 0;

    // Tick counting
    size_t   last_processed_  = 0;
    size_t   ticks_in_window_ = 0;
    uint64_t window_start_ns_ = 0;
    bool     window_started_  = false;

    void flush_window();
};

}  // namespace qf::signals
