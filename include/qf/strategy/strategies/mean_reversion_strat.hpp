#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "qf/common/config.hpp"
#include "qf/common/types.hpp"
#include "qf/signals/composite/regime_detector.hpp"
#include "qf/strategy/strategy_base.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// MeanReversionStrategy — fades extreme moves by buying when the z-score
// falls below -z_entry and selling when it rises above +z_entry. Positions
// are exited when the z-score crosses back through z_exit (toward the mean).
//
// The strategy only trades when RegimeDetector reports MarketRegime::Ranging.
//
// Configurable parameters:
//   z_entry       — z-score threshold to enter a position (default 2.0)
//   z_exit        — z-score threshold to exit a position (default 0.5)
//   lookback      — number of price samples for mean/stddev (default 50)
//   max_position  — maximum position size in either direction (default 1000)
class MeanReversionStrategy : public StrategyBase {
public:
    MeanReversionStrategy();

    std::vector<Decision> on_signal(const signals::Signal& signal,
                                    const PortfolioView& portfolio) override;

    std::string name() override;

    void configure(const Config& config) override;

    // Inject a regime detector (for dependency injection / testing).
    void set_regime_detector(signals::RegimeDetector* detector);

    // --- Accessors (for testing) ---
    double   z_entry() const { return z_entry_; }
    double   z_exit() const { return z_exit_; }
    uint32_t lookback() const { return lookback_; }
    int64_t  max_position() const { return max_position_; }

private:
    // Configuration
    double   z_entry_{2.0};
    double   z_exit_{0.5};
    uint32_t lookback_{50};
    int64_t  max_position_{1000};

    // External dependency (non-owning)
    signals::RegimeDetector* regime_detector_{nullptr};

    // Per-symbol rolling price window for z-score computation
    std::unordered_map<uint64_t, std::deque<double>> price_windows_;

    // Compute z-score for a symbol given the current price.
    // Returns 0.0 if insufficient data.
    double compute_zscore(uint64_t symbol_key, double current_price);
};

}  // namespace qf::strategy
