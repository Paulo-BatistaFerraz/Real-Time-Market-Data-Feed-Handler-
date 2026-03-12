#include "qf/signals/features/volatility_estimator.hpp"
#include <cmath>

namespace qf::signals {

VolatilityEstimator::VolatilityEstimator(size_t window_size,
                                          double annualization_factor)
    : window_size_(window_size)
    , annualization_factor_(annualization_factor) {}

double VolatilityEstimator::compute(const core::OrderBook& /*book*/,
                                     const TradeHistory& trades) {
    const auto& trade_vec = trades.trades();

    // Ingest new trades into the current bar
    for (size_t i = last_processed_; i < trade_vec.size(); ++i) {
        double px = price_to_double(trade_vec[i].price);
        if (!bar_started_) {
            cur_open_ = cur_high_ = cur_low_ = cur_close_ = px;
            bar_started_ = true;
        } else {
            if (px > cur_high_) cur_high_ = px;
            if (px < cur_low_)  cur_low_  = px;
            cur_close_ = px;
        }
    }
    last_processed_ = trade_vec.size();

    // Flush the accumulated trades as one bar
    if (bar_started_) {
        flush_bar();
    }

    recompute();

    return parkinson_vol_;
}

std::string VolatilityEstimator::name() {
    return "volatility_estimator";
}

void VolatilityEstimator::flush_bar() {
    OHLCBar bar{cur_open_, cur_high_, cur_low_, cur_close_};

    if (bars_.size() >= window_size_) {
        bars_.pop_front();
    }
    bars_.push_back(bar);

    bar_started_ = false;
    cur_open_ = cur_high_ = cur_low_ = cur_close_ = 0.0;
}

void VolatilityEstimator::recompute() {
    size_t n = bars_.size();
    parkinson_vol_  = 0.0;
    yang_zhang_vol_ = 0.0;

    if (n < 2) {
        return;
    }

    // --- Parkinson estimator ---
    // vol = sqrt( annualization / (4 * n * ln2) * sum( (ln(H_i/L_i))^2 ) )
    double sum_hl_sq = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double hl = std::log(bars_[i].high / bars_[i].low);
        sum_hl_sq += hl * hl;
    }
    double ln2 = std::log(2.0);
    parkinson_vol_ = std::sqrt(annualization_factor_ / (4.0 * n * ln2) * sum_hl_sq);

    // --- Yang-Zhang estimator ---
    // Requires at least 3 bars (m >= 2 return observations) for sample variance.
    if (n < 3) {
        return;
    }

    // m = n - 1 (number of return observations using bars[i-1].close as prev close)
    size_t m = n - 1;

    // Overnight returns: o_i = ln(O_i / C_{i-1})
    // Close-to-close returns: c_i = ln(C_i / C_{i-1})
    double sum_o = 0.0;
    double sum_c = 0.0;
    for (size_t i = 1; i < n; ++i) {
        sum_o += std::log(bars_[i].open / bars_[i - 1].close);
        sum_c += std::log(bars_[i].close / bars_[i - 1].close);
    }
    double mean_o = sum_o / m;
    double mean_c = sum_c / m;

    // Overnight variance (sample variance, divide by m-1)
    double var_o = 0.0;
    for (size_t i = 1; i < n; ++i) {
        double d = std::log(bars_[i].open / bars_[i - 1].close) - mean_o;
        var_o += d * d;
    }
    var_o /= (m - 1);

    // Close-to-close variance
    double var_c = 0.0;
    for (size_t i = 1; i < n; ++i) {
        double d = std::log(bars_[i].close / bars_[i - 1].close) - mean_c;
        var_c += d * d;
    }
    var_c /= (m - 1);

    // Rogers-Satchell variance
    double var_rs = 0.0;
    for (size_t i = 1; i < n; ++i) {
        double lnhc = std::log(bars_[i].high / bars_[i].close);
        double lnho = std::log(bars_[i].high / bars_[i].open);
        double lnlc = std::log(bars_[i].low / bars_[i].close);
        double lnlo = std::log(bars_[i].low / bars_[i].open);
        var_rs += lnhc * lnho + lnlc * lnlo;
    }
    var_rs /= m;

    // k = 0.34 / (1.34 + (m+1)/(m-1))
    double k = 0.34 / (1.34 + static_cast<double>(m + 1) / static_cast<double>(m - 1));

    double var_yz = var_o + k * var_c + (1.0 - k) * var_rs;
    if (var_yz < 0.0) {
        var_yz = 0.0;
    }
    yang_zhang_vol_ = std::sqrt(annualization_factor_ * var_yz);
}

}  // namespace qf::signals
