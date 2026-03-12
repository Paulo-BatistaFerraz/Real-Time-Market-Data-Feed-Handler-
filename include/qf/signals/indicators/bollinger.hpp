#pragma once

#include "qf/signals/indicators/indicator_base.hpp"
#include <string>
#include <vector>

namespace qf::signals {

// Result struct returned by Bollinger::update().
struct BollingerResult {
    double upper;      // middle + k * stddev
    double middle;     // SMA over the rolling window
    double lower;      // middle - k * stddev
    double percent_b;  // %B = (price - lower) / (upper - lower), or 0 if bandwidth==0
    double bandwidth;  // (upper - lower) / middle, or 0 if middle==0
};

// Bollinger Bands indicator.
// Computes a Simple Moving Average (middle band), upper band (middle + k*std),
// and lower band (middle - k*std) over a rolling window of prices.
class Bollinger : public IndicatorBase {
public:
    explicit Bollinger(int period = 20, double num_std = 2.0);

    // Feed a new price. Updates rolling window and returns the middle band value.
    // Use result() to get the full BollingerResult after calling update().
    double update(double price) override;

    // Returns the current middle band value.
    double value() const override;

    void reset() override;
    std::string name() const override;

    // Returns the full Bollinger result from the most recent update().
    const BollingerResult& result() const { return result_; }

    int period() const { return period_; }
    double num_std() const { return num_std_; }
    bool ready() const { return count_ >= period_; }

private:
    int period_;
    double num_std_;
    std::vector<double> window_;
    int head_ = 0;       // circular buffer write position
    int count_ = 0;      // number of values received so far
    double sum_ = 0.0;
    double sum_sq_ = 0.0;
    BollingerResult result_{};
};

}  // namespace qf::signals
