#include <gtest/gtest.h>
#include "qf/simulator/scenario_loader.hpp"

using namespace qf;
using namespace qf::simulator;

// --- Valid JSON parsing ---

TEST(ScenarioLoader, ParsesValidJsonWithAllFields) {
    std::string json = R"({
        "symbols": ["AAPL", "MSFT"],
        "initial_prices": {"AAPL": 1500000, "MSFT": 3000000},
        "multicast_address": "239.255.0.5",
        "port": 40001,
        "messages_per_second": 5000,
        "duration_seconds": 60,
        "seed": 123
    })";

    SimConfig cfg = ScenarioLoader::parse(json);

    EXPECT_EQ(cfg.symbols.size(), 2u);
    EXPECT_EQ(cfg.symbols[0].name, "AAPL");
    EXPECT_EQ(cfg.symbols[1].name, "MSFT");
    EXPECT_DOUBLE_EQ(cfg.symbols[0].start_price, 1500000.0);
    EXPECT_DOUBLE_EQ(cfg.symbols[1].start_price, 3000000.0);
    EXPECT_EQ(cfg.multicast_ip, "239.255.0.5");
    EXPECT_EQ(cfg.port, 40001);
    EXPECT_EQ(cfg.msg_rate, 5000u);
    EXPECT_EQ(cfg.duration_sec, 60u);
    EXPECT_EQ(cfg.seed, 123u);
}

TEST(ScenarioLoader, ParsesMinimalValidJson) {
    std::string json = R"({
        "symbols": ["SPY"],
        "initial_prices": {"SPY": 4500000}
    })";

    SimConfig cfg = ScenarioLoader::parse(json);

    EXPECT_EQ(cfg.symbols.size(), 1u);
    EXPECT_EQ(cfg.symbols[0].name, "SPY");
    EXPECT_EQ(cfg.multicast_ip, MULTICAST_GROUP);
    EXPECT_EQ(cfg.port, MULTICAST_PORT);
    EXPECT_EQ(cfg.msg_rate, DEFAULT_MSG_RATE);
    EXPECT_EQ(cfg.duration_sec, DEFAULT_DURATION);
    EXPECT_EQ(cfg.seed, DEFAULT_SEED);
}

TEST(ScenarioLoader, InitialPricesMapPopulated) {
    std::string json = R"({
        "symbols": ["AAPL", "GOOG"],
        "initial_prices": {"AAPL": 1500000, "GOOG": 2800000}
    })";

    SimConfig cfg = ScenarioLoader::parse(json);

    EXPECT_EQ(cfg.initial_prices.size(), 2u);
    EXPECT_EQ(cfg.initial_prices.at("AAPL"), 1500000u);
    EXPECT_EQ(cfg.initial_prices.at("GOOG"), 2800000u);
}

TEST(ScenarioLoader, SymbolConfigHasDefaults) {
    std::string json = R"({
        "symbols": ["TSLA"],
        "initial_prices": {"TSLA": 7000000}
    })";

    SimConfig cfg = ScenarioLoader::parse(json);

    EXPECT_DOUBLE_EQ(cfg.symbols[0].volatility, 0.001);
    EXPECT_DOUBLE_EQ(cfg.symbols[0].tick_size, 100.0);
}

// --- Throws on missing/invalid required fields ---

TEST(ScenarioLoader, ThrowsOnMissingSymbols) {
    std::string json = R"({
        "initial_prices": {"AAPL": 1500000}
    })";
    EXPECT_THROW(ScenarioLoader::parse(json), std::runtime_error);
}

TEST(ScenarioLoader, ThrowsOnEmptySymbols) {
    std::string json = R"({
        "symbols": [],
        "initial_prices": {"AAPL": 1500000}
    })";
    EXPECT_THROW(ScenarioLoader::parse(json), std::runtime_error);
}

TEST(ScenarioLoader, ThrowsOnMissingInitialPrices) {
    std::string json = R"({
        "symbols": ["AAPL"]
    })";
    EXPECT_THROW(ScenarioLoader::parse(json), std::runtime_error);
}

TEST(ScenarioLoader, ThrowsOnSymbolWithoutPrice) {
    std::string json = R"({
        "symbols": ["AAPL", "MSFT"],
        "initial_prices": {"AAPL": 1500000}
    })";
    EXPECT_THROW(ScenarioLoader::parse(json), std::runtime_error);
}

TEST(ScenarioLoader, ThrowsOnInvalidJson) {
    std::string json = "{ not valid json }}}";
    EXPECT_THROW(ScenarioLoader::parse(json), std::runtime_error);
}

TEST(ScenarioLoader, ThrowsOnSymbolsNotArray) {
    std::string json = R"({
        "symbols": "AAPL",
        "initial_prices": {"AAPL": 1500000}
    })";
    EXPECT_THROW(ScenarioLoader::parse(json), std::runtime_error);
}

TEST(ScenarioLoader, ThrowsOnInitialPricesNotObject) {
    std::string json = R"({
        "symbols": ["AAPL"],
        "initial_prices": [1500000]
    })";
    EXPECT_THROW(ScenarioLoader::parse(json), std::runtime_error);
}

TEST(ScenarioLoader, ThrowsOnInvalidFilePath) {
    EXPECT_THROW(ScenarioLoader::load("nonexistent.json"), std::runtime_error);
}
