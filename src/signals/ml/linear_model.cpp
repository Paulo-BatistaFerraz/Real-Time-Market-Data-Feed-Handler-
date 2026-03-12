#include "qf/signals/ml/linear_model.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace qf::signals {

LinearModel::LinearModel(size_t num_features, double lambda)
    : num_features_(num_features)
    , lambda_(lambda)
    , weights_(num_features, 0.0)
    , P_(num_features * num_features, 0.0)
{
    // Initialize P = (1/lambda) * I
    double inv_lambda = 1.0 / lambda_;
    for (size_t i = 0; i < num_features_; ++i) {
        P(i, i) = inv_lambda;
    }
}

void LinearModel::train(const std::vector<double>& features, double target) {
    assert(features.size() == num_features_);
    ++num_observations_;

    // Recursive Least Squares (RLS) update:
    // 1. k = P * x / (1 + x^T * P * x)
    // 2. error = target - predict(features)
    // 3. weights += k * error
    // 4. P -= k * (x^T * P)

    // Compute P * x
    std::vector<double> Px(num_features_, 0.0);
    for (size_t i = 0; i < num_features_; ++i) {
        double sum = 0.0;
        for (size_t j = 0; j < num_features_; ++j) {
            sum += P(i, j) * features[j];
        }
        Px[i] = sum;
    }

    // Compute x^T * P * x = x^T * Px
    double xPx = 0.0;
    for (size_t i = 0; i < num_features_; ++i) {
        xPx += features[i] * Px[i];
    }

    // Kalman gain: k = Px / (1 + xPx)
    double denom = 1.0 + xPx;
    std::vector<double> k(num_features_);
    for (size_t i = 0; i < num_features_; ++i) {
        k[i] = Px[i] / denom;
    }

    // Prediction error
    double prediction = predict(features);
    double error = target - prediction;

    // Update weights
    for (size_t i = 0; i < num_features_; ++i) {
        weights_[i] += k[i] * error;
    }

    // Update bias with simple running correction
    bias_ += error / static_cast<double>(num_observations_);

    // Update P: P -= k * (x^T * P)
    // First compute x^T * P (a row vector), which is the same as Px transposed
    // since P is symmetric. But let's compute it properly for numerical stability.
    std::vector<double> xTP(num_features_, 0.0);
    for (size_t j = 0; j < num_features_; ++j) {
        double sum = 0.0;
        for (size_t i = 0; i < num_features_; ++i) {
            sum += features[i] * P(i, j);
        }
        xTP[j] = sum;
    }

    // P -= k * xTP^T (outer product)
    for (size_t i = 0; i < num_features_; ++i) {
        for (size_t j = 0; j < num_features_; ++j) {
            P(i, j) -= k[i] * xTP[j];
        }
    }
}

double LinearModel::predict(const std::vector<double>& features) const {
    assert(features.size() == num_features_);

    double result = bias_;
    for (size_t i = 0; i < num_features_; ++i) {
        result += weights_[i] * features[i];
    }
    return result;
}

void LinearModel::set_weights(const std::vector<double>& w) {
    assert(w.size() == num_features_);
    weights_ = w;
}

void LinearModel::reset() {
    std::fill(weights_.begin(), weights_.end(), 0.0);
    std::fill(P_.begin(), P_.end(), 0.0);
    bias_ = 0.0;
    num_observations_ = 0;

    double inv_lambda = 1.0 / lambda_;
    for (size_t i = 0; i < num_features_; ++i) {
        P(i, i) = inv_lambda;
    }
}

}  // namespace qf::signals
