#pragma once

#include <vector>
#include "qf/common/types.hpp"
#include "qf/risk/risk_types.hpp"
#include "qf/risk/monitors/pnl_monitor.hpp"
#include "qf/risk/monitors/exposure_monitor.hpp"
#include "qf/oms/oms_types.hpp"
#include "qf/strategy/position/portfolio.hpp"

namespace qf::risk {

// Configuration for the PostTradeRisk orchestrator.
struct PostTradeRiskConfig {
    // Maximum daily loss (positive number, e.g. 50000.0 = $50k).
    double max_daily_loss{50'000.0};

    // PnL warning threshold (0–1).  Alert fires at this fraction of max loss.
    double pnl_warning_threshold{0.8};

    // Maximum net exposure (dollars).
    double max_net_exposure{1'000'000.0};

    // Maximum gross exposure (dollars).
    double max_gross_exposure{2'000'000.0};
};

// PostTradeRisk — orchestrates post-trade risk monitors.
//
// After every fill, updates all monitors and checks whether any risk
// limit is NOW breached (the limit may have been fine pre-trade).
// Returns a list of all breached limits.
class PostTradeRisk {
public:
    explicit PostTradeRisk(const PostTradeRiskConfig& config);

    // Process a fill: update all monitors, check for breaches.
    // Returns a (possibly empty) list of breached/warning results.
    std::vector<RiskResult> on_fill(const oms::FillReport& fill,
                                    const strategy::Portfolio& portfolio);

    // Set reference prices for exposure calculations.
    void set_price(const Symbol& symbol, Price price);

    // Access individual monitors.
    PnLMonitor&      pnl_monitor()      { return pnl_monitor_; }
    ExposureMonitor& exposure_monitor()  { return exposure_monitor_; }

    const PnLMonitor&      pnl_monitor()      const { return pnl_monitor_; }
    const ExposureMonitor& exposure_monitor()  const { return exposure_monitor_; }

private:
    PnLMonitor      pnl_monitor_;
    ExposureMonitor exposure_monitor_;
};

}  // namespace qf::risk
