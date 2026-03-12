#include "qf/risk/monitors/exposure_monitor.hpp"
#include "qf/common/logging.hpp"

#include <cmath>
#include <sstream>

namespace qf::risk {

ExposureMonitor::ExposureMonitor(double max_net_exposure, double max_gross_exposure)
    : max_net_(max_net_exposure)
    , max_gross_(max_gross_exposure) {}

void ExposureMonitor::set_price(const Symbol& symbol, Price price) {
    prices_[symbol.as_key()] = price;
}

void ExposureMonitor::update(const strategy::Portfolio& portfolio) {
    auto snap = portfolio.snapshot();

    exposure_ = Exposure{};

    for (const auto& [symbol_key, position] : snap.positions) {
        auto price_it = prices_.find(symbol_key);
        if (price_it == prices_.end()) continue;

        double notional = static_cast<double>(position)
                        * price_to_double(price_it->second);

        exposure_.per_symbol[symbol_key] = notional;
        exposure_.net_exposure += notional;
        exposure_.gross_exposure += std::abs(notional);
    }
}

RiskResult ExposureMonitor::check() const {
    double abs_net = std::abs(exposure_.net_exposure);

    // Check gross exposure first (more restrictive).
    if (exposure_.gross_exposure > max_gross_) {
        std::ostringstream msg;
        msg << "Gross exposure " << exposure_.gross_exposure
            << " exceeds limit " << max_gross_;
        return RiskResult{
            RiskCheck::ExposureLimit,
            false,
            exposure_.gross_exposure,
            max_gross_,
            msg.str()
        };
    }

    // Check net exposure.
    if (abs_net > max_net_) {
        std::ostringstream msg;
        msg << "Net exposure " << abs_net << " exceeds limit " << max_net_;
        return RiskResult{
            RiskCheck::ExposureLimit,
            false,
            abs_net,
            max_net_,
            msg.str()
        };
    }

    return RiskResult{
        RiskCheck::ExposureLimit,
        true,
        exposure_.gross_exposure,
        max_gross_,
        ""
    };
}

double ExposureMonitor::symbol_net_exposure(const Symbol& symbol) const {
    auto it = exposure_.per_symbol.find(symbol.as_key());
    return (it != exposure_.per_symbol.end()) ? it->second : 0.0;
}

}  // namespace qf::risk
