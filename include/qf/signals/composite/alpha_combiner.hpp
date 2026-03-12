#pragma once

#include <string>
#include <unordered_map>
#include "qf/signals/signal_types.hpp"

namespace qf::signals {

// AlphaCombiner — takes a weighted linear combination of normalized feature
// values from a FeatureVector and produces a composite signal clamped to [-1, +1].
//
// Usage:
//   AlphaCombiner combiner;
//   combiner.set_weight("momentum", 0.4);
//   combiner.set_weight("mean_reversion", -0.3);
//   combiner.set_weight("ofi", 0.3);
//   double alpha = combiner.combine(feature_vector);
//
// Features not present in the FeatureVector are treated as 0.
class AlphaCombiner {
public:
    AlphaCombiner() = default;

    // Set the weight for a named feature.
    void set_weight(const std::string& feature_name, double weight);

    // Remove a feature weight.
    void remove_weight(const std::string& feature_name);

    // Set all weights at once, replacing any existing weights.
    void set_weights(const std::unordered_map<std::string, double>& weights);

    // Get the weight for a feature (0 if not set).
    double get_weight(const std::string& feature_name) const;

    // Return a const reference to all weights.
    const std::unordered_map<std::string, double>& weights() const { return weights_; }

    // Compute the weighted combination of features, clamped to [-1, +1].
    double combine(const FeatureVector& features) const;

    // Number of configured feature weights.
    size_t size() const { return weights_.size(); }

private:
    std::unordered_map<std::string, double> weights_;

    // Clamp value to [-1, +1].
    static double clamp(double value);
};

}  // namespace qf::signals
