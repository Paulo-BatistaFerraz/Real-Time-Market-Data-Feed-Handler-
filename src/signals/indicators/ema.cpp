#include "qf/signals/indicators/ema.hpp"

namespace qf::signals {

EMA::EMA(int span)
    : span_(span)
    , alpha_(2.0 / (static_cast<double>(span) + 1.0)) {}

double EMA::update(double value) {
    if (!initialized_) {
        ema_ = value;
        initialized_ = true;
    } else {
        ema_ = alpha_ * value + (1.0 - alpha_) * ema_;
    }
    return ema_;
}

double EMA::value() const {
    return ema_;
}

void EMA::reset() {
    ema_ = 0.0;
    initialized_ = false;
}

std::string EMA::name() const {
    return "ema_" + std::to_string(span_);
}

}  // namespace qf::signals
