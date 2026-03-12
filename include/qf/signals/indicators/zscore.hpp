#pragma once

#include "qf/signals/indicators/indicator_base.hpp"
#include <string>
#include <vector>

namespace qf::signals {

// Rolling Z-Score indicator.
// Computes (value - mean) / std over a configurable window using
// Welford's online algorithm for numerically stable mean and variance.
// Returns 0 if insufficient data (fewer than 2 samples) or zero standard deviation.
class ZScore : public IndicatorBase {
public:
    explicit ZScore(int window = 20);

    // Feed a new value. Returns the current z-score.
    double update(double value) override;

    // Returns the current z-score without consuming new data.
    double value() const override;

    void reset() override;
    std::string name() const override;

    int window() const { return window_; }
    bool ready() const { return count_ >= 2; }

private:
    int window_;
    std::vector<double> buf_;  // circular buffer
    int head_ = 0;             // write position
    int count_ = 0;            // values received so far (capped at window_)

    // Welford accumulators
    double mean_ = 0.0;
    double m2_ = 0.0;         // sum of squared deviations from mean

    double zscore_ = 0.0;
};

}  // namespace qf::signals
