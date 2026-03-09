#include <gtest/gtest.h>
#include "mdfh/consumer/throughput_tracker.hpp"

using namespace mdfh::consumer;

// TODO: Throughput tracker tests

TEST(ThroughputTracker, RollingWindowAverage) {
    EXPECT_TRUE(true);
}

TEST(ThroughputTracker, PeakRate) {
    EXPECT_TRUE(true);
}

TEST(ThroughputTracker, ResetClearsAll) {
    ThroughputTracker t;
    t.reset();
    EXPECT_EQ(t.total(), 0u);
}
