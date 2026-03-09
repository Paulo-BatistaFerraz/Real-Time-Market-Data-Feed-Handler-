#include <gtest/gtest.h>
#include "mdfh/consumer/latency_histogram.hpp"

using namespace mdfh::consumer;

// TODO: Latency histogram tests

TEST(LatencyHistogram, RecordAndPercentile) {
    EXPECT_TRUE(true);
}

TEST(LatencyHistogram, MinMaxMean) {
    EXPECT_TRUE(true);
}

TEST(LatencyHistogram, ResetClearsAll) {
    LatencyHistogram h;
    h.reset();
    EXPECT_EQ(h.count(), 0u);
}
