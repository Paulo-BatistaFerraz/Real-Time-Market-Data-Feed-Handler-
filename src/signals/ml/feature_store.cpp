#include "qf/signals/ml/feature_store.hpp"
#include <algorithm>
#include <set>

namespace qf::signals {

FeatureStore::FeatureStore(size_t max_depth)
    : max_depth_(max_depth) {}

void FeatureStore::push(const FeatureVector& fv) {
    if (buffer_.size() >= max_depth_) {
        buffer_.pop_front();
    }
    buffer_.push_back(fv);
}

std::vector<FeatureVector> FeatureStore::get_window(size_t n) const {
    if (n >= buffer_.size()) {
        return {buffer_.begin(), buffer_.end()};
    }
    return {buffer_.end() - static_cast<std::ptrdiff_t>(n), buffer_.end()};
}

std::vector<std::vector<double>> FeatureStore::get_matrix(
    size_t n,
    std::vector<std::string>& columns_out) const {

    auto window = get_window(n);
    if (window.empty()) {
        columns_out.clear();
        return {};
    }

    // Collect the union of all feature names across the window, sorted for
    // deterministic column ordering.
    std::set<std::string> col_set;
    for (const auto& fv : window) {
        for (const auto& [name, _] : fv.values) {
            col_set.insert(name);
        }
    }
    columns_out.assign(col_set.begin(), col_set.end());

    // Build the matrix: rows = time steps, cols = features.
    std::vector<std::vector<double>> matrix;
    matrix.reserve(window.size());
    for (const auto& fv : window) {
        std::vector<double> row;
        row.reserve(columns_out.size());
        for (const auto& col : columns_out) {
            row.push_back(fv.get(col, 0.0));
        }
        matrix.push_back(std::move(row));
    }
    return matrix;
}

void FeatureStore::clear() {
    buffer_.clear();
}

}  // namespace qf::signals
