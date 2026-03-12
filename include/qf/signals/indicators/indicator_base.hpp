#pragma once

#include <string>

namespace qf::signals {

// Abstract base class for derived indicators (EMA, RSI, Bollinger, etc.).
//
// Indicators are stateful: they consume a stream of values (typically feature
// outputs) and maintain internal state to produce a smoothed/transformed output.
// Unlike features, indicators do not see the order book directly — they operate
// on scalar time series.
class IndicatorBase {
public:
    virtual ~IndicatorBase() = default;

    // Feed a new value and update internal state. Returns the new indicator value.
    virtual double update(double value) = 0;

    // Current indicator value without consuming new data.
    virtual double value() const = 0;

    // Reset internal state to initial conditions.
    virtual void reset() = 0;

    // Unique name identifying this indicator (e.g. "ema_20", "rsi_14").
    virtual std::string name() const = 0;
};

}  // namespace qf::signals
