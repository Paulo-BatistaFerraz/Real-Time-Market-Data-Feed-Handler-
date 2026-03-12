#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "qf/consumer/stats_collector.hpp"

namespace qf::consumer {

enum class AlertLevel : uint8_t {
    Info,
    Warning,
    Critical
};

struct Alert {
    AlertLevel  level;
    std::string message;
    uint64_t    timestamp_ns;
};

struct AlertConfig {
    uint64_t p99_threshold_us    = 100;    // warn if p99 > 100μs
    double   rate_drop_pct       = 50.0;   // warn if rate drops > 50%
    uint64_t stall_timeout_ms    = 2000;   // warn if no msgs for 2s
    bool     alert_on_gaps       = true;
};

class AlertMonitor {
public:
    explicit AlertMonitor(const AlertConfig& config = {});

    // Check snapshot against rules, return any triggered alerts
    std::vector<Alert> check(const StatsSnapshot& snap);

    uint64_t total_alerts() const { return total_alerts_; }

private:
    AlertConfig config_;
    double last_rate_ = 0.0;
    uint64_t last_gap_count_ = 0;
    uint64_t total_alerts_ = 0;
};

}  // namespace qf::consumer
