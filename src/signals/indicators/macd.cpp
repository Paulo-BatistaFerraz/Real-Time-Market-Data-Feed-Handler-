#include "qf/signals/indicators/macd.hpp"

namespace qf::signals {

MACD::MACD(int fast_period, int slow_period, int signal_period)
    : fast_period_(fast_period)
    , slow_period_(slow_period)
    , signal_period_(signal_period)
    , fast_ema_(fast_period)
    , slow_ema_(slow_period)
    , signal_ema_(signal_period) {}

double MACD::update(double price) {
    double fast_val = fast_ema_.update(price);
    double slow_val = slow_ema_.update(price);
    double macd_line = fast_val - slow_val;
    double signal_line = signal_ema_.update(macd_line);
    double histogram = macd_line - signal_line;

    result_ = MACDResult{macd_line, signal_line, histogram};
    return macd_line;
}

double MACD::value() const {
    return result_.macd_line;
}

void MACD::reset() {
    fast_ema_.reset();
    slow_ema_.reset();
    signal_ema_.reset();
    result_ = MACDResult{};
}

std::string MACD::name() const {
    return "macd_" + std::to_string(fast_period_) + "_" +
           std::to_string(slow_period_) + "_" + std::to_string(signal_period_);
}

}  // namespace qf::signals
