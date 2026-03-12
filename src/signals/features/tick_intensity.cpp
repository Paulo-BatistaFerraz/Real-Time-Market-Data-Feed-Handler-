#include "qf/signals/features/tick_intensity.hpp"
#include <cmath>

namespace qf::signals {

TickIntensity::TickIntensity(double alpha, uint64_t window_ns)
    : alpha_(alpha)
    , window_ns_(window_ns) {}

double TickIntensity::compute(const core::OrderBook& /*book*/,
                               const TradeHistory& trades) {
    const auto& trade_vec = trades.trades();

    // Process new trades since last compute
    for (size_t i = last_processed_; i < trade_vec.size(); ++i) {
        uint64_t ts = trade_vec[i].timestamp;

        if (!window_started_) {
            window_start_ns_ = ts;
            window_started_ = true;
            ticks_in_window_ = 0;
        }

        // Check if this trade falls outside the current window
        while (window_started_ && ts >= window_start_ns_ + window_ns_) {
            // Flush the completed window
            flush_window();
            // Start a new window
            window_start_ns_ += window_ns_;
            ticks_in_window_ = 0;
        }

        ++ticks_in_window_;
    }
    last_processed_ = trade_vec.size();

    // Compute current rate from the in-progress window
    // Use ticks accumulated so far as the current rate estimate
    current_rate_ = static_cast<double>(ticks_in_window_);

    // Compute ratio: current rate vs baseline
    if (!baseline_initialized_ || baseline_rate_ <= 0.0) {
        // No baseline yet — return 1.0 (neutral)
        spike_magnitude_ = 0.0;
        return 1.0;
    }

    double ratio = current_rate_ / baseline_rate_;

    // Spike magnitude: deviation in standard deviations
    double stddev = std::sqrt(baseline_variance_);
    if (stddev > 0.0) {
        spike_magnitude_ = (current_rate_ - baseline_rate_) / stddev;
    } else {
        spike_magnitude_ = 0.0;
    }

    return ratio;
}

std::string TickIntensity::name() {
    return "tick_intensity";
}

void TickIntensity::flush_window() {
    double rate = static_cast<double>(ticks_in_window_);
    ++window_count_;

    if (!baseline_initialized_) {
        // First window: initialize EWMA directly
        baseline_rate_ = rate;
        baseline_variance_ = 0.0;
        baseline_initialized_ = true;
    } else {
        // Update EWMA of rate
        double diff = rate - baseline_rate_;
        baseline_rate_ += alpha_ * diff;

        // Update EWMA of variance: Var = (1-alpha)*Var + alpha*(rate - mean)^2
        baseline_variance_ = (1.0 - alpha_) * baseline_variance_ + alpha_ * diff * diff;
    }
}

}  // namespace qf::signals
