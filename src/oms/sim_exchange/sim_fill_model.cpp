#include "qf/oms/sim_exchange/sim_fill_model.hpp"

#include <algorithm>
#include <numeric>

namespace qf::oms {

SimFillModel::SimFillModel(const FillModelConfig& config)
    : config_(config), rng_(std::random_device{}()) {}

bool SimFillModel::should_reject() {
    return uniform_(rng_) < config_.reject_rate;
}

Timestamp SimFillModel::apply_latency(Timestamp base_timestamp) const {
    return base_timestamp + config_.latency_ns;
}

std::vector<Quantity> SimFillModel::split_quantity(Quantity total) {
    // Small orders or low probability → single fill
    if (total <= config_.partial_fill_threshold || uniform_(rng_) >= config_.partial_fill_prob) {
        return {total};
    }

    // Determine number of partial fills
    std::uniform_int_distribution<uint32_t> count_dist(
        config_.min_partial_fills, config_.max_partial_fills);
    uint32_t num_fills = count_dist(rng_);
    num_fills = std::min(num_fills, total); // can't have more fills than quantity

    std::vector<Quantity> fills;
    fills.reserve(num_fills);

    Quantity remaining = total;
    for (uint32_t i = 0; i < num_fills - 1; ++i) {
        // Each partial fill gets at least 1 unit, leave at least 1 for remaining fills
        uint32_t max_this = remaining - (num_fills - 1 - i);
        std::uniform_int_distribution<Quantity> qty_dist(1, std::max(Quantity{1}, max_this));
        Quantity fill_qty = qty_dist(rng_);
        fills.push_back(fill_qty);
        remaining -= fill_qty;
    }
    // Last fill gets the remainder
    fills.push_back(remaining);

    return fills;
}

}  // namespace qf::oms
