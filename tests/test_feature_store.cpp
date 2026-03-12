#include <gtest/gtest.h>
#include "qf/signals/ml/feature_store.hpp"
#include "qf/signals/ml/feature_normalizer.hpp"
#include <cmath>

using namespace qf::signals;

// ---------------------------------------------------------------------------
// FeatureStore tests
// ---------------------------------------------------------------------------

TEST(FeatureStore, PushAndSize) {
    FeatureStore store(5);
    EXPECT_EQ(store.size(), 0u);
    EXPECT_EQ(store.max_depth(), 5u);
    EXPECT_FALSE(store.full());

    FeatureVector fv;
    fv.set("a", 1.0);
    fv.timestamp = 100;
    store.push(fv);
    EXPECT_EQ(store.size(), 1u);
    EXPECT_FALSE(store.full());
}

TEST(FeatureStore, EvictsOldestWhenFull) {
    FeatureStore store(3);
    for (int i = 0; i < 5; ++i) {
        FeatureVector fv;
        fv.set("x", static_cast<double>(i));
        fv.timestamp = static_cast<uint64_t>(i);
        store.push(fv);
    }
    EXPECT_EQ(store.size(), 3u);
    EXPECT_TRUE(store.full());

    // Oldest should be i=2 (0 and 1 evicted)
    auto window = store.get_window(3);
    EXPECT_EQ(window.size(), 3u);
    EXPECT_DOUBLE_EQ(window[0].get("x"), 2.0);
    EXPECT_DOUBLE_EQ(window[1].get("x"), 3.0);
    EXPECT_DOUBLE_EQ(window[2].get("x"), 4.0);
}

TEST(FeatureStore, GetWindowReturnsLastN) {
    FeatureStore store(10);
    for (int i = 0; i < 7; ++i) {
        FeatureVector fv;
        fv.set("val", static_cast<double>(i));
        store.push(fv);
    }
    auto window = store.get_window(3);
    EXPECT_EQ(window.size(), 3u);
    EXPECT_DOUBLE_EQ(window[0].get("val"), 4.0);
    EXPECT_DOUBLE_EQ(window[1].get("val"), 5.0);
    EXPECT_DOUBLE_EQ(window[2].get("val"), 6.0);
}

TEST(FeatureStore, GetWindowClampedToSize) {
    FeatureStore store(10);
    FeatureVector fv;
    fv.set("a", 1.0);
    store.push(fv);

    auto window = store.get_window(100);
    EXPECT_EQ(window.size(), 1u);
}

TEST(FeatureStore, GetMatrixConsistentColumns) {
    FeatureStore store(10);

    FeatureVector fv1;
    fv1.set("alpha", 1.0);
    fv1.set("beta", 2.0);
    store.push(fv1);

    FeatureVector fv2;
    fv2.set("alpha", 3.0);
    fv2.set("gamma", 4.0);  // beta missing, gamma new
    store.push(fv2);

    std::vector<std::string> columns;
    auto matrix = store.get_matrix(2, columns);

    EXPECT_EQ(columns.size(), 3u);  // alpha, beta, gamma (sorted)
    EXPECT_EQ(columns[0], "alpha");
    EXPECT_EQ(columns[1], "beta");
    EXPECT_EQ(columns[2], "gamma");

    EXPECT_EQ(matrix.size(), 2u);
    // Row 0: fv1 — alpha=1, beta=2, gamma=0 (missing)
    EXPECT_DOUBLE_EQ(matrix[0][0], 1.0);
    EXPECT_DOUBLE_EQ(matrix[0][1], 2.0);
    EXPECT_DOUBLE_EQ(matrix[0][2], 0.0);
    // Row 1: fv2 — alpha=3, beta=0 (missing), gamma=4
    EXPECT_DOUBLE_EQ(matrix[1][0], 3.0);
    EXPECT_DOUBLE_EQ(matrix[1][1], 0.0);
    EXPECT_DOUBLE_EQ(matrix[1][2], 4.0);
}

TEST(FeatureStore, LatestReturnsNewest) {
    FeatureStore store(5);
    for (int i = 0; i < 3; ++i) {
        FeatureVector fv;
        fv.set("v", static_cast<double>(i));
        fv.timestamp = static_cast<uint64_t>(i * 100);
        store.push(fv);
    }
    EXPECT_DOUBLE_EQ(store.latest().get("v"), 2.0);
    EXPECT_EQ(store.latest().timestamp, 200u);
}

TEST(FeatureStore, ClearResetsStore) {
    FeatureStore store(5);
    FeatureVector fv;
    fv.set("a", 1.0);
    store.push(fv);
    store.push(fv);
    store.clear();
    EXPECT_EQ(store.size(), 0u);
    EXPECT_FALSE(store.full());
}

// ---------------------------------------------------------------------------
// FeatureNormalizer tests
// ---------------------------------------------------------------------------

TEST(FeatureNormalizer, WelfordMeanVariance) {
    FeatureNormalizer norm;
    // Feed values: 2, 4, 4, 4, 5, 5, 7, 9
    double values[] = {2, 4, 4, 4, 5, 5, 7, 9};
    for (double v : values) {
        norm.update("x", v);
    }
    auto& s = norm.stats("x");
    EXPECT_EQ(s.count, 8u);
    EXPECT_DOUBLE_EQ(s.mean, 5.0);
    // Population variance = 4.0
    EXPECT_NEAR(s.variance(), 4.0, 1e-10);
    EXPECT_NEAR(s.stddev(), 2.0, 1e-10);
}

TEST(FeatureNormalizer, NormalizeZScore) {
    FeatureNormalizer norm;
    double values[] = {2, 4, 4, 4, 5, 5, 7, 9};
    for (double v : values) {
        norm.update("x", v);
    }
    // mean=5, std=2: normalize(7) = (7-5)/2 = 1.0
    EXPECT_NEAR(norm.normalize("x", 7.0), 1.0, 1e-10);
    // normalize(3) = (3-5)/2 = -1.0
    EXPECT_NEAR(norm.normalize("x", 3.0), -1.0, 1e-10);
    // normalize(5) = 0
    EXPECT_NEAR(norm.normalize("x", 5.0), 0.0, 1e-10);
}

TEST(FeatureNormalizer, InsufficientDataReturnsZero) {
    FeatureNormalizer norm;
    // No data at all
    EXPECT_DOUBLE_EQ(norm.normalize("x", 42.0), 0.0);
    // One data point
    norm.update("x", 10.0);
    EXPECT_DOUBLE_EQ(norm.normalize("x", 42.0), 0.0);
}

TEST(FeatureNormalizer, ConstantInputReturnsZero) {
    FeatureNormalizer norm;
    for (int i = 0; i < 100; ++i) {
        norm.update("c", 5.0);
    }
    // stddev == 0 for constant input
    EXPECT_DOUBLE_EQ(norm.normalize("c", 5.0), 0.0);
    EXPECT_DOUBLE_EQ(norm.normalize("c", 10.0), 0.0);
}

TEST(FeatureNormalizer, UpdateFeatureVector) {
    FeatureNormalizer norm;
    for (int i = 0; i < 10; ++i) {
        FeatureVector fv;
        fv.set("a", static_cast<double>(i));
        fv.set("b", static_cast<double>(i * 2));
        norm.update(fv);
    }
    EXPECT_EQ(norm.feature_count(), 2u);
    EXPECT_EQ(norm.stats("a").count, 10u);
    EXPECT_EQ(norm.stats("b").count, 10u);
}

TEST(FeatureNormalizer, NormalizeFeatureVector) {
    FeatureNormalizer norm;
    double values[] = {2, 4, 4, 4, 5, 5, 7, 9};
    for (double v : values) {
        norm.update("x", v);
        norm.update("y", v * 2);  // mean=10, std=4
    }

    FeatureVector fv;
    fv.set("x", 7.0);   // (7-5)/2 = 1.0
    fv.set("y", 14.0);  // (14-10)/4 = 1.0
    fv.timestamp = 999;

    auto result = norm.normalize(fv);
    EXPECT_NEAR(result.get("x"), 1.0, 1e-10);
    EXPECT_NEAR(result.get("y"), 1.0, 1e-10);
    EXPECT_EQ(result.timestamp, 999u);
}

TEST(FeatureNormalizer, ResetClearsAll) {
    FeatureNormalizer norm;
    norm.update("x", 1.0);
    norm.update("x", 2.0);
    norm.reset();
    EXPECT_EQ(norm.feature_count(), 0u);
    EXPECT_DOUBLE_EQ(norm.normalize("x", 5.0), 0.0);
}

TEST(FeatureNormalizer, UnseenFeatureStatsAreZero) {
    FeatureNormalizer norm;
    auto& s = norm.stats("nonexistent");
    EXPECT_EQ(s.count, 0u);
    EXPECT_DOUBLE_EQ(s.mean, 0.0);
    EXPECT_DOUBLE_EQ(s.m2, 0.0);
}
