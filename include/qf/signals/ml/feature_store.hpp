#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <vector>
#include "qf/signals/signal_types.hpp"

namespace qf::signals {

// FeatureStore — rolling window of FeatureVectors for ML model consumption.
//
// Stores up to `max_depth` observations. push() appends a new observation and
// evicts the oldest when capacity is reached. get_window(n) returns the last n
// observations suitable for feeding into a model.
class FeatureStore {
public:
    explicit FeatureStore(size_t max_depth);

    // Add a new observation. Evicts oldest if at capacity.
    void push(const FeatureVector& fv);

    // Return the last n observations (oldest first). If n > size(), returns all.
    std::vector<FeatureVector> get_window(size_t n) const;

    // Return a matrix (rows = time steps, cols = features) of the last n
    // observations with consistent column ordering. Column names are returned
    // via `columns_out`. Missing features default to 0.
    std::vector<std::vector<double>> get_matrix(size_t n,
                                                 std::vector<std::string>& columns_out) const;

    // Current number of stored observations.
    size_t size() const { return buffer_.size(); }

    // Maximum depth (capacity).
    size_t max_depth() const { return max_depth_; }

    // Whether the store is at capacity.
    bool full() const { return buffer_.size() == max_depth_; }

    // Most recent observation.
    const FeatureVector& latest() const { return buffer_.back(); }

    // Clear all stored observations.
    void clear();

private:
    size_t max_depth_;
    std::deque<FeatureVector> buffer_;
};

}  // namespace qf::signals
