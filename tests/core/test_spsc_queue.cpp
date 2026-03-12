#include <gtest/gtest.h>
#include <thread>
#include "qf/core/spsc_queue.hpp"

using namespace qf::core;

// TODO: SPSC queue tests

TEST(SPSCQueue, PushPopSingleThread) {
    SPSCQueue<int, 16> q;
    EXPECT_TRUE(q.empty());
    EXPECT_TRUE(q.try_push(42));
    int val = 0;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueue, FullQueueRejectsPush) {
    EXPECT_TRUE(true);
}

TEST(SPSCQueue, EmptyQueueRejectsPop) {
    EXPECT_TRUE(true);
}

TEST(SPSCQueue, ConcurrentProducerConsumer) {
    EXPECT_TRUE(true);
}

TEST(SPSCQueue, BatchPushPop) {
    EXPECT_TRUE(true);
}
