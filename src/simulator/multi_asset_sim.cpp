#include "qf/simulator/multi_asset_sim.hpp"
#include <cmath>
#include <stdexcept>

namespace qf::simulator {

MultiAssetSim::MultiAssetSim(std::vector<PriceWalk>& price_walks,
                             uint32_t seed,
                             const MultiAssetConfig& config)
    : price_walks_(price_walks)
    , rng_(seed)
{
    size_t n = price_walks_.size();
    if (n == 0) {
        return;
    }

    if (config.correlation_matrix.empty()) {
        // Default: independent assets (identity matrix)
        cholesky_ = identity_matrix(n);
    } else {
        if (config.correlation_matrix.size() != n) {
            throw std::invalid_argument(
                "Correlation matrix size does not match number of assets");
        }
        for (const auto& row : config.correlation_matrix) {
            if (row.size() != n) {
                throw std::invalid_argument(
                    "Correlation matrix must be square");
            }
        }
        cholesky_ = cholesky_decompose(config.correlation_matrix);
    }
}

void MultiAssetSim::step() {
    size_t n = price_walks_.size();
    if (n == 0) return;

    // Step 1: Generate N independent standard normal draws
    std::vector<double> z(n);
    for (size_t i = 0; i < n; ++i) {
        z[i] = normal_(rng_);
    }

    // Step 2: Apply Cholesky factor to produce correlated draws
    // correlated[i] = sum_j(L[i][j] * z[j])
    std::vector<double> correlated(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j <= i; ++j) {
            correlated[i] += cholesky_[i][j] * z[j];
        }
    }

    // Step 3: Advance each price walk with its correlated noise
    for (size_t i = 0; i < n; ++i) {
        price_walks_[i].next(correlated[i]);
    }
}

std::vector<std::vector<double>> MultiAssetSim::cholesky_decompose(
    const std::vector<std::vector<double>>& matrix)
{
    size_t n = matrix.size();
    std::vector<std::vector<double>> L(n, std::vector<double>(n, 0.0));

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j <= i; ++j) {
            double sum = 0.0;

            if (j == i) {
                // Diagonal element
                for (size_t k = 0; k < j; ++k) {
                    sum += L[j][k] * L[j][k];
                }
                double val = matrix[j][j] - sum;
                if (val < 0.0) {
                    throw std::invalid_argument(
                        "Correlation matrix is not positive semi-definite");
                }
                L[i][j] = std::sqrt(val);
            } else {
                // Off-diagonal element
                for (size_t k = 0; k < j; ++k) {
                    sum += L[i][k] * L[j][k];
                }
                if (L[j][j] == 0.0) {
                    L[i][j] = 0.0;
                } else {
                    L[i][j] = (matrix[i][j] - sum) / L[j][j];
                }
            }
        }
    }

    return L;
}

std::vector<std::vector<double>> MultiAssetSim::identity_matrix(size_t n) {
    std::vector<std::vector<double>> I(n, std::vector<double>(n, 0.0));
    for (size_t i = 0; i < n; ++i) {
        I[i][i] = 1.0;
    }
    return I;
}

}  // namespace qf::simulator
