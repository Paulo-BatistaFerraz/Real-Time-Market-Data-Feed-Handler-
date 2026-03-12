#include "qf/strategy/execution/slippage_model.hpp"

#include <cmath>

namespace qf::strategy {

SlippageModel::SlippageModel(double alpha) : alpha_(alpha) {}

void SlippageModel::set_alpha(double alpha) { alpha_ = alpha; }

void SlippageModel::set_adv(const Symbol& symbol, double adv) {
    adv_[symbol.as_key()] = adv;
}

double SlippageModel::estimate_slippage(const Symbol& symbol,
                                        Quantity quantity) const {
    const double volume = adv(symbol);
    if (volume < 1e-9) {
        return 0.0;
    }
    return alpha_ * (static_cast<double>(quantity) / volume);
}

Price SlippageModel::estimated_fill_price(const Symbol& symbol,
                                          Side side,
                                          Price reference_price,
                                          Quantity quantity) const {
    const double slip = estimate_slippage(symbol, quantity);
    const double ref  = price_to_double(reference_price);

    double fill = 0.0;
    if (side == Side::Buy) {
        // Buyer pays more.
        fill = ref * (1.0 + slip);
    } else {
        // Seller receives less.
        fill = ref * (1.0 - slip);
    }

    // Clamp to non-negative.
    if (fill < 0.0) {
        fill = 0.0;
    }

    return double_to_price(fill);
}

double SlippageModel::adv(const Symbol& symbol) const {
    auto it = adv_.find(symbol.as_key());
    return (it != adv_.end()) ? it->second : DEFAULT_ADV;
}

void SlippageModel::configure(const Config& config) {
    alpha_ = config.get<double>("slippage.alpha", 0.1);
}

}  // namespace qf::strategy
