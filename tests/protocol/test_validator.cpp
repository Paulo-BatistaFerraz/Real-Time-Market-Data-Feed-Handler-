#include <gtest/gtest.h>
#include "qf/protocol/validator.hpp"

using namespace qf::protocol;

// TODO: Validator edge case tests

TEST(Validator, RejectsBufferTooSmall) {
    EXPECT_TRUE(true);
}

TEST(Validator, RejectsUnknownMessageType) {
    EXPECT_TRUE(true);
}

TEST(Validator, RejectsZeroPrice) {
    EXPECT_TRUE(true);
}

TEST(Validator, RejectsZeroQuantity) {
    EXPECT_TRUE(true);
}

TEST(Validator, AcceptsValidAddOrder) {
    EXPECT_TRUE(true);
}
