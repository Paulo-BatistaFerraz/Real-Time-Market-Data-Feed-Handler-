#include "qf/signals/ml/feature_normalizer.hpp"
#include <cmath>

namespace qf::signals {

const FeatureStats FeatureNormalizer::empty_stats_{};

double FeatureStats::stddev() const {
    double var = variance();
    return (var > 0.0) ? std::sqrt(var) : 0.0;
}

void FeatureNormalizer::update(const std::string& feature_name, double value) {
    auto& s = stats_[feature_name];
    ++s.count;
    double delta = value - s.mean;
    s.mean += delta / static_cast<double>(s.count);
    double delta2 = value - s.mean;
    s.m2 += delta * delta2;
}

double FeatureNormalizer::normalize(const std::string& feature_name, double raw_value) const {
    auto it = stats_.find(feature_name);
    if (it == stats_.end() || it->second.count < 2) {
        return 0.0;
    }
    double sd = it->second.stddev();
    if (sd == 0.0) {
        return 0.0;
    }
    return (raw_value - it->second.mean) / sd;
}

void FeatureNormalizer::update(const FeatureVector& fv) {
    for (const auto& [name, value] : fv.values) {
        update(name, value);
    }
}

FeatureVector FeatureNormalizer::normalize(const FeatureVector& fv) const {
    FeatureVector result;
    result.timestamp = fv.timestamp;
    for (const auto& [name, value] : fv.values) {
        result.set(name, normalize(name, value));
    }
    return result;
}

const FeatureStats& FeatureNormalizer::stats(const std::string& feature_name) const {
    auto it = stats_.find(feature_name);
    return (it != stats_.end()) ? it->second : empty_stats_;
}

void FeatureNormalizer::reset() {
    stats_.clear();
}

}  // namespace qf::signals
