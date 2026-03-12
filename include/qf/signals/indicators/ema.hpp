#pragma once

#include "qf/signals/indicators/indicator_base.hpp"
#include <string>

namespace qf::signals {

// Exponential Moving Average indicator.
// Smooths a scalar time series using the formula:
//   ema = alpha * value + (1 - alpha) * prev_ema
// where alpha = 2 / (span + 1).
// The first value initializes the EMA directly (no warm-up bias).
class EMA : public IndicatorBase {
public:
    explicit EMA(int span);

    double update(double value) override;
    double value() const override;
    void reset() override;
    std::string name() const override;

private:
    int span_;
    double alpha_;
    double ema_ = 0.0;
    bool initialized_ = false;
};

}  // namespace qf::signals
