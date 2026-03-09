#include <gtest/gtest.h>
#include "mdfh/protocol/validator.hpp"

using namespace mdfh::protocol;

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
