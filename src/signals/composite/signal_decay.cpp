#include "qf/signals/composite/signal_decay.hpp"
#include <cmath>
#include <stdexcept>

namespace qf::signals {

SignalDecay::SignalDecay(double half_life_seconds)
    : half_life_seconds_(half_life_seconds)
    , lambda_(0.0) {
    if (half_life_seconds <= 0.0) {
        throw std::invalid_argument("SignalDecay half-life must be > 0");
    }
    recompute_lambda();
}

void SignalDecay::set_half_life(double half_life_seconds) {
    if (half_life_seconds <= 0.0) {
        throw std::invalid_argument("SignalDecay half-life must be > 0");
    }
    half_life_seconds_ = half_life_seconds;
    recompute_lambda();
}

double SignalDecay::decay(double signal_strength, double elapsed_seconds) const {
    if (elapsed_seconds <= 0.0) {
        return signal_strength;
    }
    return signal_strength * std::exp(-lambda_ * elapsed_seconds);
}

double SignalDecay::decay_ns(double signal_strength, uint64_t signal_time, uint64_t current_time) const {
    if (current_time <= signal_time) {
        return signal_strength;
    }
    double elapsed_seconds = static_cast<double>(current_time - signal_time) / 1.0e9;
    return decay(signal_strength, elapsed_seconds);
}

double SignalDecay::decay_factor(double elapsed_seconds) const {
    if (elapsed_seconds <= 0.0) {
        return 1.0;
    }
    return std::exp(-lambda_ * elapsed_seconds);
}

double SignalDecay::time_to_threshold(double signal_strength, double threshold) const {
    double abs_signal = std::fabs(signal_strength);
    if (abs_signal <= threshold) {
        return 0.0;  // already below threshold
    }
    if (threshold <= 0.0) {
        return std::numeric_limits<double>::infinity();  // never reaches zero exactly
    }
    // |signal| * exp(-lambda * t) = threshold
    // t = -ln(threshold / |signal|) / lambda
    return -std::log(threshold / abs_signal) / lambda_;
}

void SignalDecay::recompute_lambda() {
    lambda_ = std::log(2.0) / half_life_seconds_;
}

}  // namespace qf::signals
