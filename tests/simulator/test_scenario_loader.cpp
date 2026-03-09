#include <gtest/gtest.h>
#include "mdfh/simulator/scenario_loader.hpp"

using namespace mdfh::simulator;

// TODO: Scenario loader tests

TEST(ScenarioLoader, LoadsValidJson) {
    EXPECT_TRUE(true);
}

TEST(ScenarioLoader, DefaultsForMissingFields) {
    EXPECT_TRUE(true);
}

TEST(ScenarioLoader, ThrowsOnInvalidPath) {
    EXPECT_THROW(ScenarioLoader::load("nonexistent.json"), std::runtime_error);
}
