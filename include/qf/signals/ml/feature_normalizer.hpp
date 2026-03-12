#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include "qf/signals/signal_types.hpp"

namespace qf::signals {

// Per-feature running statistics using Welford's online algorithm.
struct FeatureStats {
    size_t count = 0;
    double mean  = 0.0;
    double m2    = 0.0;   // sum of squared deviations from mean

    double variance() const {
        return (count < 2) ? 0.0 : m2 / static_cast<double>(count);
    }

    double stddev() const;
};

// FeatureNormalizer — online z-score normalization using Welford's algorithm.
//
// Maintains running mean and variance per feature name. update() ingests a new
// raw value and updates statistics. normalize() returns (value - mean) / std.
// When stddev is zero (insufficient data or constant input), normalize returns 0.
class FeatureNormalizer {
public:
    FeatureNormalizer() = default;

    // Update running statistics for a single feature.
    void update(const std::string& feature_name, double value);

    // Normalize a raw value: (value - mean) / stddev.
    // Returns 0 if fewer than 2 observations or stddev == 0.
    double normalize(const std::string& feature_name, double raw_value) const;

    // Update all features in a FeatureVector.
    void update(const FeatureVector& fv);

    // Normalize all features in a FeatureVector, returning a new FeatureVector.
    FeatureVector normalize(const FeatureVector& fv) const;

    // Get current stats for a feature. Returns default (zero) stats if unseen.
    const FeatureStats& stats(const std::string& feature_name) const;

    // Number of tracked features.
    size_t feature_count() const { return stats_.size(); }

    // Reset all statistics.
    void reset();

private:
    std::unordered_map<std::string, FeatureStats> stats_;
    static const FeatureStats empty_stats_;
};

}  // namespace qf::signals
