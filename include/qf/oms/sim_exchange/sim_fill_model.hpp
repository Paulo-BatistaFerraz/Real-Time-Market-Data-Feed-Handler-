#pragma once

#include <cstdint>
#include <random>
#include <vector>
#include "qf/oms/oms_types.hpp"

namespace qf::oms {

// Configuration for realistic fill behavior.
struct FillModelConfig {
    uint64_t latency_ns{50000};         // simulated round-trip latency in nanoseconds
    double   partial_fill_prob{0.3};    // probability a large order gets partial fills
    uint32_t min_partial_fills{2};      // minimum number of fills when splitting
    uint32_t max_partial_fills{5};      // maximum number of fills when splitting
    double   reject_rate{0.02};         // probability of random rejection (0.0 - 1.0)
    uint32_t partial_fill_threshold{100}; // quantity above which partial fills can occur
};

// SimFillModel adds realistic behavior to simulated fills:
//   - configurable latency (simulated round-trip)
//   - partial fills (split large orders)
//   - random rejects (configurable rate)
class SimFillModel {
public:
    explicit SimFillModel(const FillModelConfig& config = {});

    // Returns true if this order should be randomly rejected.
    bool should_reject();

    // Compute the simulated fill timestamp (original + latency).
    Timestamp apply_latency(Timestamp base_timestamp) const;

    // Split a quantity into partial fill quantities.
    // Returns a vector of quantities that sum to the original.
    // May return a single element if no split occurs.
    std::vector<Quantity> split_quantity(Quantity total);

    const FillModelConfig& config() const { return config_; }

private:
    FillModelConfig config_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_{0.0, 1.0};
};

}  // namespace qf::oms
