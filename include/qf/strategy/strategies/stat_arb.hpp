#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>
#include "qf/common/config.hpp"
#include "qf/common/types.hpp"
#include "qf/strategy/strategy_base.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// StatArbStrategy — pairs trading strategy that tracks the price ratio of
// two correlated symbols and trades the spread when it diverges from its
// historical mean by more than entry_zscore standard deviations. Positions
// are closed when the spread reverts to within exit_zscore.
//
// The strategy goes long the cheap leg and short the expensive leg when
// the spread is below -entry_zscore, and the reverse when above +entry_zscore.
//
// Configurable parameters:
//   symbol_a      — first symbol in the pair (default "AAPL")
//   symbol_b      — second symbol in the pair (default "MSFT")
//   entry_zscore  — z-score threshold to enter the spread trade (default 2.0)
//   exit_zscore   — z-score threshold to exit the spread trade (default 0.5)
//   lookback      — number of ratio samples for mean/stddev (default 50)
//   trade_size    — quantity per leg (default 100)
class StatArbStrategy : public StrategyBase {
public:
    StatArbStrategy();

    std::vector<Decision> on_signal(const signals::Signal& signal,
                                    const PortfolioView& portfolio) override;

    std::string name() override;

    void configure(const Config& config) override;

    // --- Accessors (for testing) ---
    const std::string& symbol_a() const { return symbol_a_; }
    const std::string& symbol_b() const { return symbol_b_; }
    double   entry_zscore() const { return entry_zscore_; }
    double   exit_zscore() const { return exit_zscore_; }
    uint32_t lookback() const { return lookback_; }
    Quantity trade_size() const { return trade_size_; }

private:
    // Configuration
    std::string symbol_a_{"AAPL"};
    std::string symbol_b_{"MSFT"};
    double   entry_zscore_{2.0};
    double   exit_zscore_{0.5};
    uint32_t lookback_{50};
    Quantity trade_size_{100};

    // Last observed price per symbol (keyed by symbol_key)
    double last_price_a_{0.0};
    double last_price_b_{0.0};

    // Rolling window of price ratios (A / B)
    std::deque<double> ratio_window_;

    // Track whether we currently hold a spread position and its direction.
    // +1 = long A / short B,  -1 = short A / long B,  0 = flat
    int spread_position_{0};

    // Compute z-score of the current ratio relative to the rolling window.
    double compute_ratio_zscore(double current_ratio) const;

    // Helper: is this signal for symbol A or B?
    bool is_symbol_a(const signals::Signal& signal) const;
    bool is_symbol_b(const signals::Signal& signal) const;
};

}  // namespace qf::strategy
