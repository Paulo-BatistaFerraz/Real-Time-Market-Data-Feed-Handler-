#include <gtest/gtest.h>
#include <vector>
#include "qf/data/storage/bar_aggregator.hpp"
#include "qf/common/types.hpp"

using namespace qf;
using namespace qf::data;

namespace {

Symbol make_symbol(const char* name) {
    Symbol s{};
    for (int i = 0; i < 8 && name[i]; ++i) s.data[i] = name[i];
    return s;
}

}  // namespace

// --- Bar aggregator produces correct OHLCV from known ticks ---
//
// Hand computation for one-second bar:
//   Ticks (price in fixed-point, qty):
//     t=0.1s : price=1000, qty=10  (open)
//     t=0.3s : price=1200, qty=20  (high)
//     t=0.5s : price= 800, qty=15  (low)
//     t=0.9s : price=1100, qty=25  (close)
//
//   Expected OHLCV: O=1000, H=1200, L=800, C=1100, V=70, ticks=4

TEST(BarAggregatorTest, SingleBarOHLCV) {
    std::vector<Bar> emitted;
    BarAggregator agg(BarInterval::OneSecond, [&](const Bar& bar) {
        emitted.push_back(bar);
    });

    Symbol sym = make_symbol("AAPL");

    // All within the [0, 1s) interval.
    agg.add_tick(sym, 1000, 10, 100'000'000ULL);   // 0.1s
    agg.add_tick(sym, 1200, 20, 300'000'000ULL);   // 0.3s
    agg.add_tick(sym,  800, 15, 500'000'000ULL);   // 0.5s
    agg.add_tick(sym, 1100, 25, 900'000'000ULL);   // 0.9s

    // No bar emitted yet (still in same interval).
    EXPECT_EQ(emitted.size(), 0u);

    // Verify incomplete bar state.
    const Bar* current = agg.current_bar(sym);
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->open, 1000u);
    EXPECT_EQ(current->high, 1200u);
    EXPECT_EQ(current->low,   800u);
    EXPECT_EQ(current->close, 1100u);
    EXPECT_EQ(current->volume, 70u);
    EXPECT_EQ(current->tick_count, 4u);

    // Flush to emit.
    agg.flush();
    ASSERT_EQ(emitted.size(), 1u);

    const auto& bar = emitted[0];
    EXPECT_EQ(bar.symbol, sym);
    EXPECT_EQ(bar.open, 1000u);
    EXPECT_EQ(bar.high, 1200u);
    EXPECT_EQ(bar.low,   800u);
    EXPECT_EQ(bar.close, 1100u);
    EXPECT_EQ(bar.volume, 70u);
    EXPECT_EQ(bar.tick_count, 4u);
    EXPECT_EQ(bar.start_time, 0ULL);  // aligned to 0
}

TEST(BarAggregatorTest, IntervalBoundaryEmitsBar) {
    std::vector<Bar> emitted;
    BarAggregator agg(BarInterval::OneSecond, [&](const Bar& bar) {
        emitted.push_back(bar);
    });

    Symbol sym = make_symbol("MSFT");

    // First bar: [0, 1s)
    agg.add_tick(sym, 500, 5, 100'000'000ULL);   // 0.1s
    agg.add_tick(sym, 600, 8, 800'000'000ULL);   // 0.8s

    // Second bar: [1s, 2s) — crossing the boundary auto-emits the first bar.
    agg.add_tick(sym, 700, 3, 1'200'000'000ULL); // 1.2s
    ASSERT_EQ(emitted.size(), 1u);

    // Verify the first bar.
    EXPECT_EQ(emitted[0].open, 500u);
    EXPECT_EQ(emitted[0].high, 600u);
    EXPECT_EQ(emitted[0].low,  500u);
    EXPECT_EQ(emitted[0].close, 600u);
    EXPECT_EQ(emitted[0].volume, 13u);
    EXPECT_EQ(emitted[0].tick_count, 2u);

    // The second bar is still open.
    const Bar* current = agg.current_bar(sym);
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->open, 700u);
    EXPECT_EQ(current->tick_count, 1u);
}

TEST(BarAggregatorTest, MultiSymbolBarsIndependent) {
    std::vector<Bar> emitted;
    BarAggregator agg(BarInterval::OneSecond, [&](const Bar& bar) {
        emitted.push_back(bar);
    });

    Symbol aapl = make_symbol("AAPL");
    Symbol msft = make_symbol("MSFT");

    // AAPL ticks in [0, 1s)
    agg.add_tick(aapl, 1000, 10, 200'000'000ULL);
    agg.add_tick(aapl, 1050, 20, 500'000'000ULL);

    // MSFT ticks in [0, 1s)
    agg.add_tick(msft, 3000, 5,  300'000'000ULL);
    agg.add_tick(msft, 2900, 15, 700'000'000ULL);

    // Flush both.
    agg.flush();
    ASSERT_EQ(emitted.size(), 2u);

    // Find each by symbol.
    const Bar* aapl_bar = nullptr;
    const Bar* msft_bar = nullptr;
    for (const auto& b : emitted) {
        if (b.symbol == aapl) aapl_bar = &b;
        if (b.symbol == msft) msft_bar = &b;
    }

    ASSERT_NE(aapl_bar, nullptr);
    EXPECT_EQ(aapl_bar->open, 1000u);
    EXPECT_EQ(aapl_bar->high, 1050u);
    EXPECT_EQ(aapl_bar->low,  1000u);
    EXPECT_EQ(aapl_bar->close, 1050u);
    EXPECT_EQ(aapl_bar->volume, 30u);

    ASSERT_NE(msft_bar, nullptr);
    EXPECT_EQ(msft_bar->open, 3000u);
    EXPECT_EQ(msft_bar->high, 3000u);
    EXPECT_EQ(msft_bar->low,  2900u);
    EXPECT_EQ(msft_bar->close, 2900u);
    EXPECT_EQ(msft_bar->volume, 20u);
}

TEST(BarAggregatorTest, FiveMinuteInterval) {
    std::vector<Bar> emitted;
    BarAggregator agg(BarInterval::FiveMinute, [&](const Bar& bar) {
        emitted.push_back(bar);
    });

    Symbol sym = make_symbol("GOOG");
    uint64_t five_min_ns = 300'000'000'000ULL;

    // Ticks within [0, 5min)
    agg.add_tick(sym, 100, 1, 1'000'000'000ULL);         // 1s
    agg.add_tick(sym, 200, 2, 60'000'000'000ULL);        // 60s
    agg.add_tick(sym, 150, 3, 120'000'000'000ULL);       // 120s

    // Cross into [5min, 10min) to emit first bar.
    agg.add_tick(sym, 250, 4, five_min_ns + 1'000'000ULL);

    ASSERT_EQ(emitted.size(), 1u);
    EXPECT_EQ(emitted[0].open, 100u);
    EXPECT_EQ(emitted[0].high, 200u);
    EXPECT_EQ(emitted[0].low,  100u);
    EXPECT_EQ(emitted[0].close, 150u);
    EXPECT_EQ(emitted[0].volume, 6u);
    EXPECT_EQ(emitted[0].tick_count, 3u);
    EXPECT_EQ(emitted[0].start_time, 0ULL);
}

TEST(BarAggregatorTest, SingleTickBar) {
    std::vector<Bar> emitted;
    BarAggregator agg(BarInterval::OneSecond, [&](const Bar& bar) {
        emitted.push_back(bar);
    });

    Symbol sym = make_symbol("TSLA");
    agg.add_tick(sym, 4200, 100, 500'000'000ULL);
    agg.flush();

    ASSERT_EQ(emitted.size(), 1u);
    EXPECT_EQ(emitted[0].open, 4200u);
    EXPECT_EQ(emitted[0].high, 4200u);
    EXPECT_EQ(emitted[0].low,  4200u);
    EXPECT_EQ(emitted[0].close, 4200u);
    EXPECT_EQ(emitted[0].volume, 100u);
    EXPECT_EQ(emitted[0].tick_count, 1u);
}
