#pragma once

#include <cstdint>
#include <random>
#include <vector>
#include "qf/simulator/price_walk.hpp"

namespace qf::simulator {

// Configuration for correlated multi-asset price simulation
struct MultiAssetConfig {
    // Correlation matrix (N x N, symmetric positive semi-definite)
    // If empty, defaults to identity (independent assets)
    std::vector<std::vector<double>> correlation_matrix;
};

// Generates correlated price moves across multiple symbols
// Uses Cholesky decomposition of the correlation matrix to transform
// independent standard normal draws into correlated draws
class MultiAssetSim {
public:
    MultiAssetSim(std::vector<PriceWalk>& price_walks,
                  uint32_t seed,
                  const MultiAssetConfig& config = {});

    // Advance all price walks with correlated random draws
    void step();

    // Number of assets being simulated
    size_t asset_count() const { return price_walks_.size(); }

    // Access the Cholesky lower-triangular factor (for testing/inspection)
    const std::vector<std::vector<double>>& cholesky_factor() const {
        return cholesky_;
    }

private:
    std::vector<PriceWalk>& price_walks_;
    std::vector<std::vector<double>> cholesky_;  // Lower-triangular Cholesky factor

    std::mt19937 rng_;
    std::normal_distribution<double> normal_{0.0, 1.0};

    // Compute Cholesky decomposition of correlation matrix
    // Returns lower-triangular matrix L such that L * L^T = correlation_matrix
    static std::vector<std::vector<double>> cholesky_decompose(
        const std::vector<std::vector<double>>& matrix);

    // Build default identity correlation matrix for n assets
    static std::vector<std::vector<double>> identity_matrix(size_t n);
};

}  // namespace qf::simulator
