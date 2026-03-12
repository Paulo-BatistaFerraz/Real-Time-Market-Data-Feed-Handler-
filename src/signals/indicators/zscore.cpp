#include "qf/signals/indicators/zscore.hpp"
#include <cmath>

namespace qf::signals {

ZScore::ZScore(int window)
    : window_(window)
    , buf_(static_cast<size_t>(window), 0.0) {}

double ZScore::update(double value) {
    if (count_ < window_) {
        // Window not yet full — standard Welford add
        buf_[static_cast<size_t>(head_)] = value;
        head_ = (head_ + 1) % window_;
        ++count_;

        double delta = value - mean_;
        mean_ += delta / count_;
        double delta2 = value - mean_;
        m2_ += delta * delta2;
    } else {
        // Window full — remove oldest, then add new
        double old_val = buf_[static_cast<size_t>(head_)];
        buf_[static_cast<size_t>(head_)] = value;
        head_ = (head_ + 1) % window_;

        // Update mean and m2 for the swap (remove old, add new)
        // Combined formula derived from Welford's:
        //   new_mean = old_mean + (new_val - old_val) / n
        //   m2 += (new_val - old_val) * (new_val - new_mean + old_val - old_mean)
        double delta = value - old_val;
        double old_mean = mean_;
        mean_ += delta / count_;
        m2_ += delta * (value - mean_ + old_val - old_mean);

        // Guard against floating-point drift producing tiny negatives
        if (m2_ < 0.0) m2_ = 0.0;
    }

    // Compute z-score
    if (count_ < 2) {
        zscore_ = 0.0;
    } else {
        double variance = m2_ / count_;
        if (variance <= 0.0) {
            zscore_ = 0.0;
        } else {
            double stddev = std::sqrt(variance);
            zscore_ = (value - mean_) / stddev;
        }
    }

    return zscore_;
}

double ZScore::value() const {
    return zscore_;
}

void ZScore::reset() {
    head_ = 0;
    count_ = 0;
    mean_ = 0.0;
    m2_ = 0.0;
    zscore_ = 0.0;
    for (auto& v : buf_) v = 0.0;
}

std::string ZScore::name() const {
    return "zscore_" + std::to_string(window_);
}

}  // namespace qf::signals
