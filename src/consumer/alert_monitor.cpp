#include "mdfh/consumer/alert_monitor.hpp"
#include "mdfh/common/clock.hpp"

namespace mdfh::consumer {

// TODO: Implement alert monitoring logic

AlertMonitor::AlertMonitor(const AlertConfig& config) : config_(config) {}

std::vector<Alert> AlertMonitor::check(const StatsSnapshot& snap) {
    (void)snap;
    return {};
}

}  // namespace mdfh::consumer
