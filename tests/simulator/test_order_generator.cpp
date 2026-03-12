#include <gtest/gtest.h>
#include "qf/simulator/order_generator.hpp"
#include "qf/protocol/encoder.hpp"
#include <map>

using namespace qf;
using namespace qf::simulator;

class OrderGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        symbols_.push_back(Symbol("AAPL"));
        price_walks_.emplace_back(100.0, 0.02, 0.01, 42);
    }

    std::vector<Symbol> symbols_;
    std::vector<PriceWalk> price_walks_;
    SimOrderBook book_;
};

TEST_F(OrderGeneratorTest, GeneratesMessages) {
    OrderGenerator gen(symbols_, price_walks_, book_, 42);

    uint8_t buffer[1024];
    size_t bytes = gen.next(buffer, sizeof(buffer));

    EXPECT_GT(bytes, 0u);
    EXPECT_EQ(gen.total_generated(), 1u);
}

TEST_F(OrderGeneratorTest, FirstMessageIsAlwaysAdd) {
    OrderGenerator gen(symbols_, price_walks_, book_, 42);

    uint8_t buffer[1024];
    size_t bytes = gen.next(buffer, sizeof(buffer));
    ASSERT_GT(bytes, 0u);

    auto header = protocol::Encoder::decode_header(buffer);
    EXPECT_EQ(header.message_type, AddOrder::TYPE);
}

TEST_F(OrderGeneratorTest, ProducesCorrectDistribution) {
    GeneratorConfig config;
    config.add_weight     = 0.50;
    config.cancel_weight  = 0.25;
    config.execute_weight = 0.25;
    config.replace_weight = 0.0;
    config.trade_weight   = 0.0;

    OrderGenerator gen(symbols_, price_walks_, book_, 12345, config);

    std::map<char, int> counts;
    uint8_t buffer[1024];

    // Seed the book with some orders first
    for (int i = 0; i < 50; ++i) {
        gen.next(buffer, sizeof(buffer));
    }

    // Generate a large sample and track distribution
    counts.clear();
    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        size_t bytes = gen.next(buffer, sizeof(buffer));
        if (bytes > 0) {
            auto header = protocol::Encoder::decode_header(buffer);
            counts[header.message_type]++;
        }
    }

    int total = 0;
    for (auto& [type, count] : counts) {
        total += count;
    }

    // Add messages should be roughly 50% (within tolerance)
    double add_ratio = static_cast<double>(counts['A']) / total;
    EXPECT_NEAR(add_ratio, 0.50, 0.10)
        << "Add ratio: " << add_ratio;

    // Cancel + Execute should be roughly 50% total
    double cancel_exec_ratio = static_cast<double>(counts['X'] + counts['E']) / total;
    EXPECT_NEAR(cancel_exec_ratio, 0.50, 0.10)
        << "Cancel+Execute ratio: " << cancel_exec_ratio;
}

TEST_F(OrderGeneratorTest, EmptyBookForcesAdd) {
    GeneratorConfig config;
    config.add_weight     = 0.0;
    config.cancel_weight  = 0.5;
    config.execute_weight = 0.5;
    config.replace_weight = 0.0;
    config.trade_weight   = 0.0;

    OrderGenerator gen(symbols_, price_walks_, book_, 99, config);

    uint8_t buffer[1024];
    size_t bytes = gen.next(buffer, sizeof(buffer));
    ASSERT_GT(bytes, 0u);

    auto header = protocol::Encoder::decode_header(buffer);
    EXPECT_EQ(header.message_type, AddOrder::TYPE);
}

TEST_F(OrderGeneratorTest, GeneratesMultipleTypesOverTime) {
    OrderGenerator gen(symbols_, price_walks_, book_, 77);

    std::map<char, int> counts;
    uint8_t buffer[1024];

    for (int i = 0; i < 5000; ++i) {
        size_t bytes = gen.next(buffer, sizeof(buffer));
        if (bytes > 0) {
            auto header = protocol::Encoder::decode_header(buffer);
            counts[header.message_type]++;
        }
    }

    EXPECT_GT(counts['A'], 0) << "No Add messages generated";
    EXPECT_GT(counts['X'], 0) << "No Cancel messages generated";
    EXPECT_GT(counts['E'], 0) << "No Execute messages generated";
}

TEST_F(OrderGeneratorTest, TotalGeneratedTracksCorrectly) {
    OrderGenerator gen(symbols_, price_walks_, book_, 55);

    uint8_t buffer[1024];
    const int N = 100;
    for (int i = 0; i < N; ++i) {
        gen.next(buffer, sizeof(buffer));
    }

    EXPECT_EQ(gen.total_generated(), static_cast<uint64_t>(N));
}
