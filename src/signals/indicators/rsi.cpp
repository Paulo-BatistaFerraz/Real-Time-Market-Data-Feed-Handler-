#include "qf/signals/indicators/rsi.hpp"

namespace qf::signals {

RSI::RSI(int period)
    : period_(period) {}

double RSI::update(double price) {
    ++count_;

    if (count_ == 1) {
        // First price — no delta yet
        prev_price_ = price;
        rsi_ = 50.0;
        return rsi_;
    }

    double delta = price - prev_price_;
    prev_price_ = price;

    double gain = (delta > 0.0) ? delta : 0.0;
    double loss = (delta < 0.0) ? -delta : 0.0;

    if (count_ <= period_ + 1) {
        // Warm-up phase: accumulate gains and losses
        gain_sum_ += gain;
        loss_sum_ += loss;

        if (count_ == period_ + 1) {
            // Enough data: compute initial averages
            avg_gain_ = gain_sum_ / period_;
            avg_loss_ = loss_sum_ / period_;

            if (avg_loss_ == 0.0) {
                rsi_ = 100.0;
            } else {
                double rs = avg_gain_ / avg_loss_;
                rsi_ = 100.0 - 100.0 / (1.0 + rs);
            }
        }
        // During warm-up before we have enough data, return 50
        if (count_ < period_ + 1) {
            rsi_ = 50.0;
        }
    } else {
        // Smoothed (Wilder) update
        avg_gain_ = (avg_gain_ * (period_ - 1) + gain) / period_;
        avg_loss_ = (avg_loss_ * (period_ - 1) + loss) / period_;

        if (avg_loss_ == 0.0) {
            rsi_ = 100.0;
        } else {
            double rs = avg_gain_ / avg_loss_;
            rsi_ = 100.0 - 100.0 / (1.0 + rs);
        }
    }

    return rsi_;
}

double RSI::value() const {
    return rsi_;
}

void RSI::reset() {
    prev_price_ = 0.0;
    avg_gain_ = 0.0;
    avg_loss_ = 0.0;
    gain_sum_ = 0.0;
    loss_sum_ = 0.0;
    rsi_ = 50.0;
    count_ = 0;
}

std::string RSI::name() const {
    return "rsi_" + std::to_string(period_);
}

}  // namespace qf::signals
