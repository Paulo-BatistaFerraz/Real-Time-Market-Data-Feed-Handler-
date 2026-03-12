#pragma once

#include "qf/signals/indicators/ema.hpp"
#include "qf/signals/indicators/indicator_base.hpp"
#include <string>

namespace qf::signals {

// Result struct returned by MACD::update().
struct MACDResult {
    double macd_line;    // fast EMA - slow EMA
    double signal_line;  // EMA of the MACD line
    double histogram;    // macd_line - signal_line
};

// Moving Average Convergence Divergence (MACD) indicator.
// Uses two EMAs (fast and slow) to compute the MACD line,
// then smooths it with a signal EMA to produce signal line and histogram.
class MACD : public IndicatorBase {
public:
    explicit MACD(int fast_period = 12, int slow_period = 26, int signal_period = 9);

    // Feed a new price. Returns the MACD line value.
    // Use result() to get the full MACDResult after calling update().
    double update(double price) override;

    // Returns the current MACD line value.
    double value() const override;

    void reset() override;
    std::string name() const override;

    // Returns the full MACD result from the most recent update().
    const MACDResult& result() const { return result_; }

    int fast_period() const { return fast_period_; }
    int slow_period() const { return slow_period_; }
    int signal_period() const { return signal_period_; }

private:
    int fast_period_;
    int slow_period_;
    int signal_period_;
    EMA fast_ema_;
    EMA slow_ema_;
    EMA signal_ema_;
    MACDResult result_{};
};

}  // namespace qf::signals
