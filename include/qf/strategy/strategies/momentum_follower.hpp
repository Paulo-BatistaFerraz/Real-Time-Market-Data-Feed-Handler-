#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "qf/common/config.hpp"
#include "qf/common/types.hpp"
#include "qf/signals/composite/regime_detector.hpp"
#include "qf/strategy/strategy_base.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// MomentumFollowerStrategy — follows strong directional moves by entering
// when momentum crosses above a configurable threshold and exiting when
// it crosses below the negative threshold.
//
// Configurable parameters:
//   entry_threshold   — momentum strength threshold to enter a position (default 0.5)
//   exit_threshold    — negative momentum strength threshold to exit (default 0.3)
//   lookback_period   — number of ticks used for momentum computation (default 20)
//   max_position      — maximum position size in either direction (default 1000)
//   trailing_stop_pct — trailing stop-loss percentage (0.02 = 2%) (default 0.02)
//
// The strategy only trades when RegimeDetector reports MarketRegime::Trending.
// A trailing stop-loss exits the position if price retraces by the configured
// percentage from the peak (for longs) or trough (for shorts).
class MomentumFollowerStrategy : public StrategyBase {
public:
    MomentumFollowerStrategy();

    std::vector<Decision> on_signal(const signals::Signal& signal,
                                    const PortfolioView& portfolio) override;

    std::string name() override;

    void configure(const Config& config) override;

    // Inject a regime detector (for dependency injection / testing).
    void set_regime_detector(signals::RegimeDetector* detector);

    // --- Accessors (for testing) ---
    double   entry_threshold() const { return entry_threshold_; }
    double   exit_threshold() const { return exit_threshold_; }
    uint32_t lookback_period() const { return lookback_period_; }
    int64_t  max_position() const { return max_position_; }
    double   trailing_stop_pct() const { return trailing_stop_pct_; }

private:
    // Configuration
    double   entry_threshold_{0.5};
    double   exit_threshold_{0.3};
    uint32_t lookback_period_{20};
    int64_t  max_position_{1000};
    double   trailing_stop_pct_{0.02};

    // External dependency (non-owning)
    signals::RegimeDetector* regime_detector_{nullptr};

    // Per-symbol trailing stop state
    struct TrailingStopState {
        double peak_price{0.0};   // highest price since entry (for longs)
        double trough_price{0.0}; // lowest price since entry (for shorts)
        bool   active{false};
    };
    std::unordered_map<uint64_t, TrailingStopState> trailing_stops_;

    // Check if the trailing stop has been triggered for a position.
    bool trailing_stop_triggered(const Symbol& symbol, double current_price,
                                 int64_t position);

    // Update trailing stop tracking for a symbol.
    void update_trailing_stop(const Symbol& symbol, double current_price,
                              int64_t position);

    // Reset trailing stop state when position is closed.
    void reset_trailing_stop(const Symbol& symbol);
};

}  // namespace qf::strategy
