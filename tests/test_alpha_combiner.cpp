#include <gtest/gtest.h>
#include "qf/signals/composite/alpha_combiner.hpp"
#include <cmath>

using namespace qf::signals;

class AlphaCombinerTest : public ::testing::Test {
protected:
    AlphaCombiner combiner;
};

// Test: single feature weighted sum
TEST_F(AlphaCombinerTest, SingleFeatureWeightedSum) {
    combiner.set_weight("momentum", 0.5);

    FeatureVector fv;
    fv.set("momentum", 0.8);

    double result = combiner.combine(fv);
    EXPECT_NEAR(result, 0.4, 1e-9);
}

// Test: multiple features produce correct weighted sum
TEST_F(AlphaCombinerTest, MultipleFeatureWeightedSum) {
    combiner.set_weight("momentum", 0.4);
    combiner.set_weight("ofi", 0.3);
    combiner.set_weight("mean_reversion", -0.3);

    FeatureVector fv;
    fv.set("momentum", 0.5);
    fv.set("ofi", 0.6);
    fv.set("mean_reversion", 0.2);

    // 0.4*0.5 + 0.3*0.6 + (-0.3)*0.2 = 0.2 + 0.18 - 0.06 = 0.32
    double result = combiner.combine(fv);
    EXPECT_NEAR(result, 0.32, 1e-9);
}

// Test: result clamped to +1
TEST_F(AlphaCombinerTest, ClampedToPositiveOne) {
    combiner.set_weight("a", 2.0);
    combiner.set_weight("b", 3.0);

    FeatureVector fv;
    fv.set("a", 1.0);
    fv.set("b", 1.0);

    // raw = 2*1 + 3*1 = 5.0 → clamped to 1.0
    double result = combiner.combine(fv);
    EXPECT_DOUBLE_EQ(result, 1.0);
}

// Test: result clamped to -1
TEST_F(AlphaCombinerTest, ClampedToNegativeOne) {
    combiner.set_weight("a", -2.0);
    combiner.set_weight("b", -3.0);

    FeatureVector fv;
    fv.set("a", 1.0);
    fv.set("b", 1.0);

    // raw = -2*1 + -3*1 = -5.0 → clamped to -1.0
    double result = combiner.combine(fv);
    EXPECT_DOUBLE_EQ(result, -1.0);
}

// Test: missing features treated as 0
TEST_F(AlphaCombinerTest, MissingFeaturesAreZero) {
    combiner.set_weight("momentum", 0.5);
    combiner.set_weight("missing_feature", 0.5);

    FeatureVector fv;
    fv.set("momentum", 0.6);
    // "missing_feature" not in fv

    // 0.5*0.6 + 0.5*0.0 = 0.3
    double result = combiner.combine(fv);
    EXPECT_NEAR(result, 0.3, 1e-9);
}

// Test: empty combiner returns 0
TEST_F(AlphaCombinerTest, EmptyCombinerReturnsZero) {
    FeatureVector fv;
    fv.set("momentum", 0.5);

    double result = combiner.combine(fv);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test: empty feature vector returns 0 (all features missing → all 0)
TEST_F(AlphaCombinerTest, EmptyFeatureVectorReturnsZero) {
    combiner.set_weight("momentum", 0.5);

    FeatureVector fv;
    double result = combiner.combine(fv);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test: result is always within [-1, +1]
TEST_F(AlphaCombinerTest, ResultAlwaysInRange) {
    combiner.set_weight("a", 10.0);
    combiner.set_weight("b", -10.0);
    combiner.set_weight("c", 5.0);

    FeatureVector fv;
    fv.set("a", 1.0);
    fv.set("b", -1.0);
    fv.set("c", 1.0);

    double result = combiner.combine(fv);
    EXPECT_GE(result, -1.0);
    EXPECT_LE(result, 1.0);
}

// Test: set_weights replaces all weights
TEST_F(AlphaCombinerTest, SetWeightsReplacesAll) {
    combiner.set_weight("old", 1.0);

    std::unordered_map<std::string, double> new_weights = {{"new1", 0.3}, {"new2", 0.7}};
    combiner.set_weights(new_weights);

    EXPECT_DOUBLE_EQ(combiner.get_weight("old"), 0.0);
    EXPECT_DOUBLE_EQ(combiner.get_weight("new1"), 0.3);
    EXPECT_DOUBLE_EQ(combiner.get_weight("new2"), 0.7);
    EXPECT_EQ(combiner.size(), 2u);
}

// Test: remove_weight
TEST_F(AlphaCombinerTest, RemoveWeight) {
    combiner.set_weight("a", 1.0);
    combiner.set_weight("b", 0.5);
    combiner.remove_weight("a");

    EXPECT_DOUBLE_EQ(combiner.get_weight("a"), 0.0);
    EXPECT_EQ(combiner.size(), 1u);
}

// Test: negative weights produce negative alpha
TEST_F(AlphaCombinerTest, NegativeWeightsProduceNegativeAlpha) {
    combiner.set_weight("momentum", -0.8);

    FeatureVector fv;
    fv.set("momentum", 1.0);

    double result = combiner.combine(fv);
    EXPECT_NEAR(result, -0.8, 1e-9);
}
