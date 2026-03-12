#include "qf/consumer/alert_monitor.hpp"
#include "qf/common/clock.hpp"

namespace qf::consumer {

// TODO: Implement alert monitoring logic

AlertMonitor::AlertMonitor(const AlertConfig& config) : config_(config) {}

std::vector<Alert> AlertMonitor::check(const StatsSnapshot& snap) {
    (void)snap;
    return {};
}

}  // namespace qf::consumer
