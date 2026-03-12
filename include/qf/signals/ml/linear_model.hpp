#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace qf::signals {

// LinearModel — online Ridge regression with configurable regularization.
//
// Uses Recursive Least Squares (RLS) with a regularization parameter to
// incrementally learn weights from streaming (features, target) pairs.
// The inverse covariance matrix P is maintained via Sherman-Morrison updates,
// giving O(d^2) per training step where d = number of features.
class LinearModel {
public:
    // Construct with feature dimension and regularization strength.
    // lambda controls L2 penalty; higher values shrink weights toward zero.
    explicit LinearModel(size_t num_features, double lambda = 1.0);

    // Incrementally update weights with a new (features, target) observation.
    // features.size() must equal num_features().
    void train(const std::vector<double>& features, double target);

    // Predict target value for given features.
    // features.size() must equal num_features().
    double predict(const std::vector<double>& features) const;

    // Current weight vector (length = num_features).
    const std::vector<double>& weights() const { return weights_; }

    // Set weights directly (e.g. from a loaded model).
    void set_weights(const std::vector<double>& w);

    // Bias term (intercept). Updated during training.
    double bias() const { return bias_; }

    // Number of features the model expects.
    size_t num_features() const { return num_features_; }

    // Number of training observations seen so far.
    size_t num_observations() const { return num_observations_; }

    // Regularization strength.
    double lambda() const { return lambda_; }

    // Reset model to initial state (zero weights, identity-scaled P).
    void reset();

private:
    size_t num_features_;
    double lambda_;
    std::vector<double> weights_;   // length = num_features_
    double bias_ = 0.0;
    size_t num_observations_ = 0;

    // Inverse covariance matrix P, stored as flat row-major (num_features_ x num_features_).
    std::vector<double> P_;

    // Helper: access P_(i, j)
    double& P(size_t i, size_t j) { return P_[i * num_features_ + j]; }
    double  P(size_t i, size_t j) const { return P_[i * num_features_ + j]; }
};

}  // namespace qf::signals
