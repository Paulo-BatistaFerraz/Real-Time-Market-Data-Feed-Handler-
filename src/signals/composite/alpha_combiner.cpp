#include "qf/signals/composite/alpha_combiner.hpp"
#include <algorithm>

namespace qf::signals {

void AlphaCombiner::set_weight(const std::string& feature_name, double weight) {
    weights_[feature_name] = weight;
}

void AlphaCombiner::remove_weight(const std::string& feature_name) {
    weights_.erase(feature_name);
}

void AlphaCombiner::set_weights(const std::unordered_map<std::string, double>& weights) {
    weights_ = weights;
}

double AlphaCombiner::get_weight(const std::string& feature_name) const {
    auto it = weights_.find(feature_name);
    return (it != weights_.end()) ? it->second : 0.0;
}

double AlphaCombiner::combine(const FeatureVector& features) const {
    double sum = 0.0;
    for (const auto& [name, weight] : weights_) {
        sum += weight * features.get(name, 0.0);
    }
    return clamp(sum);
}

double AlphaCombiner::clamp(double value) {
    if (value > 1.0) return 1.0;
    if (value < -1.0) return -1.0;
    return value;
}

}  // namespace qf::signals
