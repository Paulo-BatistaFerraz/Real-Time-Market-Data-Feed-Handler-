#include "qf/signals/indicators/bollinger.hpp"
#include <cmath>

namespace qf::signals {

Bollinger::Bollinger(int period, double num_std)
    : period_(period)
    , num_std_(num_std)
    , window_(static_cast<size_t>(period), 0.0) {}

double Bollinger::update(double price) {
    // If the window is full, remove the oldest value from running sums
    if (count_ >= period_) {
        double old_val = window_[static_cast<size_t>(head_)];
        sum_ -= old_val;
        sum_sq_ -= old_val * old_val;
    }

    // Insert new value into circular buffer
    window_[static_cast<size_t>(head_)] = price;
    head_ = (head_ + 1) % period_;
    if (count_ < period_) {
        ++count_;
    }

    sum_ += price;
    sum_sq_ += price * price;

    int n = count_;
    double mean = sum_ / n;

    // Compute population standard deviation over the window
    double variance = (sum_sq_ / n) - (mean * mean);
    // Guard against floating-point rounding producing tiny negatives
    if (variance < 0.0) variance = 0.0;
    double stddev = std::sqrt(variance);

    double upper = mean + num_std_ * stddev;
    double lower = mean - num_std_ * stddev;

    double band_width = upper - lower;
    double percent_b = (band_width > 0.0) ? (price - lower) / band_width : 0.0;
    double bandwidth = (mean != 0.0) ? band_width / mean : 0.0;

    result_ = BollingerResult{upper, mean, lower, percent_b, bandwidth};
    return mean;
}

double Bollinger::value() const {
    return result_.middle;
}

void Bollinger::reset() {
    head_ = 0;
    count_ = 0;
    sum_ = 0.0;
    sum_sq_ = 0.0;
    result_ = BollingerResult{};
    for (auto& v : window_) v = 0.0;
}

std::string Bollinger::name() const {
    return "bollinger_" + std::to_string(period_);
}

}  // namespace qf::signals
