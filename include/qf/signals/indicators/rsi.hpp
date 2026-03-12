#pragma once

#include "qf/signals/indicators/indicator_base.hpp"
#include <string>

namespace qf::signals {

// Relative Strength Index (RSI) indicator.
// Uses the smoothed (Wilder) method: the first `period` values compute
// an arithmetic average of gains/losses, then subsequent values use
// exponential smoothing: avg = (prev_avg * (period-1) + current) / period.
//
// RSI = 100 - 100 / (1 + avg_gain / avg_loss)
// RSI > 70 → overbought, RSI < 30 → oversold.
class RSI : public IndicatorBase {
public:
    explicit RSI(int period = 14);

    // Feed a new price. Returns the current RSI value (0-100).
    // Returns 50.0 until enough data has been received (period + 1 prices).
    double update(double price) override;

    // Returns the current RSI value without consuming new data.
    double value() const override;

    void reset() override;
    std::string name() const override;

    int period() const { return period_; }
    bool ready() const { return count_ > period_; }

    // Convenience classification
    bool is_overbought() const { return rsi_ > 70.0; }
    bool is_oversold() const { return rsi_ < 30.0; }

private:
    int period_;
    double prev_price_ = 0.0;
    double avg_gain_ = 0.0;
    double avg_loss_ = 0.0;
    double rsi_ = 50.0;
    int count_ = 0;  // number of prices received
    double gain_sum_ = 0.0;   // used during warm-up
    double loss_sum_ = 0.0;   // used during warm-up
};

}  // namespace qf::signals
