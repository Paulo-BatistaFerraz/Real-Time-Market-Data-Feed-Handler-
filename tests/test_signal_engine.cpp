#include <gtest/gtest.h>
#include "qf/signals/signal_engine.hpp"
#include "qf/signals/features/microprice.hpp"
#include "qf/signals/features/order_flow_imbalance.hpp"
#include "qf/signals/indicators/bollinger.hpp"
#include "qf/signals/composite/alpha_combiner.hpp"
#include "qf/common/types.hpp"
#include <cmath>
#include <memory>

using namespace qf;
using namespace qf::signals;

class SignalEngineTest : public ::testing::Test {
protected:
    SignalEngine engine;
    Symbol sym{"AAPL"};
    core::OrderBook book{sym};

    void SetUp() override {
        // Register a microprice feature
        engine.registry().register_feature("microprice", std::make_unique<Microprice>());
        engine.registry().set_feature_active("microprice", true);

        // Register a composite that uses microprice
        auto combiner = std::make_unique<AlphaCombiner>();
        combiner->set_weight("microprice", 0.01);  // small weight to keep in [-1,+1]
        engine.registry().register_composite("alpha", std::move(combiner));
        engine.registry().set_composite_active("alpha", true);
    }

    core::BookUpdate make_update(uint64_t ts = 1000) {
        core::BookUpdate upd;
        upd.symbol = sym;
        upd.receive_timestamp = ts;
        upd.book_timestamp = ts;
        return upd;
    }
};

// Test: full pipeline produces signals
TEST_F(SignalEngineTest, ProducesSignals) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 500, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 500, 0);

    auto signals = engine.process(make_update(), book);
    EXPECT_EQ(signals.size(), 1u);
    EXPECT_EQ(signals[0].type, SignalType::Composite);
}

// Test: signal direction reflects feature values
TEST_F(SignalEngineTest, SignalDirectionReflectsFeatures) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 500, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 500, 0);

    auto signals = engine.process(make_update(), book);
    ASSERT_EQ(signals.size(), 1u);

    // microprice = 101.0, weighted by 0.01 → 1.01 → clamped to 1.0
    EXPECT_EQ(signals[0].direction, SignalDirection::Buy);
    EXPECT_GE(signals[0].strength, -1.0);
    EXPECT_LE(signals[0].strength, 1.0);
}

// Test: signal strength is within [-1, +1]
TEST_F(SignalEngineTest, SignalStrengthInRange) {
    book.add_order(1, Side::Buy, double_to_price(50.0), 100, 0);
    book.add_order(2, Side::Sell, double_to_price(52.0), 100, 0);

    auto signals = engine.process(make_update(), book);
    ASSERT_EQ(signals.size(), 1u);
    EXPECT_GE(signals[0].strength, -1.0);
    EXPECT_LE(signals[0].strength, 1.0);
}

// Test: last_features() populated after process
TEST_F(SignalEngineTest, LastFeaturesPopulated) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 200, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 300, 0);

    engine.process(make_update(5000), book);

    const auto& fv = engine.last_features();
    EXPECT_TRUE(fv.has("microprice"));
    EXPECT_EQ(fv.timestamp, 5000u);

    // microprice = (100*300 + 102*200) / (200+300) = (30000+20400)/500 = 100.8
    EXPECT_NEAR(fv.get("microprice"), 100.8, 1e-6);
}

// Test: symbol is propagated to signals
TEST_F(SignalEngineTest, SymbolPropagated) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 100, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    auto signals = engine.process(make_update(), book);
    ASSERT_EQ(signals.size(), 1u);
    EXPECT_EQ(signals[0].symbol, sym);
}

// Test: timestamp is propagated to signals
TEST_F(SignalEngineTest, TimestampPropagated) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 100, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    auto signals = engine.process(make_update(42000), book);
    ASSERT_EQ(signals.size(), 1u);
    EXPECT_EQ(signals[0].timestamp, 42000u);
}

// Test: no composites registered → no signals
TEST_F(SignalEngineTest, NoCompositesNoSignals) {
    SignalEngine empty_engine;
    empty_engine.registry().register_feature("microprice", std::make_unique<Microprice>());
    empty_engine.registry().set_feature_active("microprice", true);
    // No composite registered

    book.add_order(1, Side::Buy, double_to_price(100.0), 100, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    auto signals = empty_engine.process(make_update(), book);
    EXPECT_TRUE(signals.empty());
}

// Test: inactive feature is not computed
TEST_F(SignalEngineTest, InactiveFeatureNotComputed) {
    engine.registry().set_feature_active("microprice", false);

    book.add_order(1, Side::Buy, double_to_price(100.0), 100, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    engine.process(make_update(), book);
    EXPECT_FALSE(engine.last_features().has("microprice"));
}

// Test: reset clears state
TEST_F(SignalEngineTest, ResetClearsState) {
    book.add_order(1, Side::Buy, double_to_price(100.0), 100, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    engine.process(make_update(), book);
    EXPECT_TRUE(engine.last_features().has("microprice"));

    engine.reset();
    EXPECT_EQ(engine.last_features().size(), 0u);
}

// Test: multiple features and indicators through the pipeline
TEST_F(SignalEngineTest, FeaturesAndIndicatorsPipeline) {
    // Add a Bollinger indicator on microprice
    engine.registry().register_indicator("microprice:bollinger_5",
                                          std::make_unique<Bollinger>(5, 2.0));
    engine.registry().set_indicator_active("microprice:bollinger_5", true);

    book.add_order(1, Side::Buy, double_to_price(100.0), 500, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 500, 0);

    // Process multiple times to populate the Bollinger indicator
    for (int i = 0; i < 5; ++i) {
        engine.process(make_update(static_cast<uint64_t>(i * 1000)), book);
    }

    const auto& fv = engine.last_features();
    EXPECT_TRUE(fv.has("microprice"));
    EXPECT_TRUE(fv.has("microprice:bollinger_5"));
}

// Test: multiple composites produce multiple signals
TEST_F(SignalEngineTest, MultipleCompositesMultipleSignals) {
    auto combiner2 = std::make_unique<AlphaCombiner>();
    combiner2->set_weight("microprice", -0.005);
    engine.registry().register_composite("beta", std::move(combiner2));
    engine.registry().set_composite_active("beta", true);

    book.add_order(1, Side::Buy, double_to_price(100.0), 100, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    auto signals = engine.process(make_update(), book);
    EXPECT_EQ(signals.size(), 2u);
}

// Test: OFI feature with trade history integration
TEST_F(SignalEngineTest, OFIWithTradeHistory) {
    engine.registry().register_feature("ofi",
        std::make_unique<OrderFlowImbalance>(10'000'000'000ULL));
    engine.registry().set_feature_active("ofi", true);

    // Add trades to the engine's trade history
    auto& trades = engine.trade_history(sym);
    trades.add({sym, double_to_price(101.0), 300, Side::Buy, 500});
    trades.add({sym, double_to_price(101.0), 100, Side::Sell, 600});

    book.add_order(1, Side::Buy, double_to_price(100.0), 100, 0);
    book.add_order(2, Side::Sell, double_to_price(102.0), 100, 0);

    engine.process(make_update(1000), book);

    const auto& fv = engine.last_features();
    EXPECT_TRUE(fv.has("ofi"));
    // buy=300, sell=100 → OFI = (300-100)/400 = 0.5
    EXPECT_NEAR(fv.get("ofi"), 0.5, 1e-6);
}
