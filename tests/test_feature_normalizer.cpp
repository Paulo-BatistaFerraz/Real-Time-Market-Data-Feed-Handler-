#include <gtest/gtest.h>
#include "qf/signals/ml/feature_normalizer.hpp"
#include <cmath>

using namespace qf::signals;

class FeatureNormalizerTest : public ::testing::Test {
protected:
    FeatureNormalizer normalizer;
};

// Test: single observation returns 0 (need at least 2 for stddev)
TEST_F(FeatureNormalizerTest, SingleObservationReturnsZero) {
    normalizer.update("x", 5.0);
    double result = normalizer.normalize("x", 5.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test: unknown feature returns 0
TEST_F(FeatureNormalizerTest, UnknownFeatureReturnsZero) {
    double result = normalizer.normalize("unknown", 42.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test: converges to correct mean with known sequence
TEST_F(FeatureNormalizerTest, ConvergesToCorrectMean) {
    // Feed values: 10, 20, 30, 40, 50 → mean = 30
    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) {
        normalizer.update("x", v);
    }

    const auto& s = normalizer.stats("x");
    EXPECT_NEAR(s.mean, 30.0, 1e-9);
    EXPECT_EQ(s.count, 5u);
}

// Test: converges to correct stddev with known sequence
TEST_F(FeatureNormalizerTest, ConvergesToCorrectStddev) {
    // Values: 10, 20, 30, 40, 50
    // Population variance = 200, stddev = sqrt(200) ≈ 14.1421
    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) {
        normalizer.update("x", v);
    }

    const auto& s = normalizer.stats("x");
    double expected_std = std::sqrt(200.0);
    EXPECT_NEAR(s.stddev(), expected_std, 1e-9);
}

// Test: normalize produces correct z-score
TEST_F(FeatureNormalizerTest, NormalizeProducesCorrectZScore) {
    // Feed values with known mean=30, stddev=sqrt(200)
    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) {
        normalizer.update("x", v);
    }

    // z-score of 50: (50 - 30) / sqrt(200) = 20 / 14.1421... ≈ 1.4142
    double result = normalizer.normalize("x", 50.0);
    double expected = 20.0 / std::sqrt(200.0);
    EXPECT_NEAR(result, expected, 1e-9);
}

// Test: value at mean normalizes to 0
TEST_F(FeatureNormalizerTest, MeanNormalizesToZero) {
    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) {
        normalizer.update("x", v);
    }

    double result = normalizer.normalize("x", 30.0);
    EXPECT_NEAR(result, 0.0, 1e-9);
}

// Test: constant values → stddev=0 → normalize returns 0
TEST_F(FeatureNormalizerTest, ConstantValuesReturnZero) {
    for (int i = 0; i < 10; ++i) {
        normalizer.update("x", 42.0);
    }

    double result = normalizer.normalize("x", 42.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test: multiple features tracked independently
TEST_F(FeatureNormalizerTest, MultipleFeaturesIndependent) {
    for (double v : {10.0, 20.0, 30.0}) {
        normalizer.update("a", v);
    }
    for (double v : {100.0, 200.0, 300.0}) {
        normalizer.update("b", v);
    }

    const auto& sa = normalizer.stats("a");
    const auto& sb = normalizer.stats("b");
    EXPECT_NEAR(sa.mean, 20.0, 1e-9);
    EXPECT_NEAR(sb.mean, 200.0, 1e-9);
    EXPECT_EQ(normalizer.feature_count(), 2u);
}

// Test: update with FeatureVector updates all features
TEST_F(FeatureNormalizerTest, UpdateWithFeatureVector) {
    FeatureVector fv;
    fv.set("x", 10.0);
    fv.set("y", 100.0);
    normalizer.update(fv);

    FeatureVector fv2;
    fv2.set("x", 20.0);
    fv2.set("y", 200.0);
    normalizer.update(fv2);

    EXPECT_EQ(normalizer.feature_count(), 2u);
    EXPECT_NEAR(normalizer.stats("x").mean, 15.0, 1e-9);
    EXPECT_NEAR(normalizer.stats("y").mean, 150.0, 1e-9);
}

// Test: normalize FeatureVector returns normalized vector
TEST_F(FeatureNormalizerTest, NormalizeFeatureVector) {
    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) {
        FeatureVector fv;
        fv.set("x", v);
        normalizer.update(fv);
    }

    FeatureVector input;
    input.set("x", 50.0);
    input.timestamp = 12345;

    FeatureVector result = normalizer.normalize(input);
    double expected = (50.0 - 30.0) / std::sqrt(200.0);
    EXPECT_NEAR(result.get("x"), expected, 1e-9);
    EXPECT_EQ(result.timestamp, 12345u);
}

// Test: reset clears all state
TEST_F(FeatureNormalizerTest, ResetClearsState) {
    normalizer.update("x", 10.0);
    normalizer.update("x", 20.0);
    EXPECT_EQ(normalizer.feature_count(), 1u);

    normalizer.reset();
    EXPECT_EQ(normalizer.feature_count(), 0u);
    EXPECT_DOUBLE_EQ(normalizer.normalize("x", 15.0), 0.0);
}

// Test: convergence with large sample
TEST_F(FeatureNormalizerTest, ConvergesWithLargeSample) {
    // Feed 1000 values uniformly from [0, 99]
    // Mean should converge to ~49.5, stddev to ~sqrt((100^2-1)/12) ≈ 28.87
    for (int i = 0; i < 100; ++i) {
        normalizer.update("x", static_cast<double>(i));
    }

    const auto& s = normalizer.stats("x");
    EXPECT_NEAR(s.mean, 49.5, 1e-9);
    EXPECT_EQ(s.count, 100u);
    // Population variance of 0..99 = (99*100*199)/(6*100) wait, let me compute correctly
    // Var = sum((x - mean)^2) / N for population
    // For 0..99: mean=49.5, sum((i-49.5)^2) for i=0..99
    // = 2 * sum((i+0.5)^2 for i=0..49) = ... = (100^2 - 1)/12 * 100/100 wait
    // E[X^2] = (N-1)(2N-1)/6 = 99*199/6 = 3283.5, E[X]^2 = 49.5^2 = 2450.25
    // Var = 3283.5 - 2450.25 = 833.25
    double expected_std = std::sqrt(833.25);
    EXPECT_NEAR(s.stddev(), expected_std, 1e-6);
}
