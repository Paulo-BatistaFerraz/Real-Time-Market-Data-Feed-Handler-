#pragma once

#include <cstdint>

namespace qf::signals {

// SignalDecay — applies exponential time-decay to a signal strength value.
//
// Models the idea that signals lose relevance over time: a signal produced
// 10 seconds ago should carry less weight than one produced 1 second ago.
//
// The decay follows: decayed = original * exp(-lambda * elapsed)
// where lambda = ln(2) / half_life_seconds.
//
// After one half-life, the signal retains 50% of its original strength.
// After two half-lives, 25%, and so on — fading smoothly toward zero.
class SignalDecay {
public:
    // Construct with half-life in seconds (must be > 0).
    explicit SignalDecay(double half_life_seconds = 5.0);

    // Set a new half-life in seconds (must be > 0).
    void set_half_life(double half_life_seconds);

    // Get the current half-life in seconds.
    double half_life() const { return half_life_seconds_; }

    // Get the computed decay constant lambda.
    double lambda() const { return lambda_; }

    // Apply decay to a signal strength given elapsed time in seconds.
    // Returns the decayed signal value.
    double decay(double signal_strength, double elapsed_seconds) const;

    // Apply decay using nanosecond timestamps.
    // signal_time and current_time are nanoseconds since midnight.
    // If current_time <= signal_time, returns the original strength (no decay).
    double decay_ns(double signal_strength, uint64_t signal_time, uint64_t current_time) const;

    // Compute the decay factor (multiplier) for a given elapsed time in seconds.
    // Returns a value in [0, 1].
    double decay_factor(double elapsed_seconds) const;

    // Compute elapsed seconds needed for the signal to decay below a threshold.
    // Returns seconds until |signal| < threshold.
    double time_to_threshold(double signal_strength, double threshold) const;

private:
    double half_life_seconds_;
    double lambda_;  // ln(2) / half_life

    void recompute_lambda();
};

}  // namespace qf::signals
