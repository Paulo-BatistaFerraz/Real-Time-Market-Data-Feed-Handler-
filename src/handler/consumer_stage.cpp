#include "mdfh/core/pipeline.hpp"
#include "mdfh/consumer/stats_collector.hpp"
#include "mdfh/consumer/console_display.hpp"
#include "mdfh/consumer/csv_logger.hpp"
#include "mdfh/consumer/alert_monitor.hpp"

// Thread 4: Analytics Consumer
// Pops BookUpdates from Q3, feeds to StatsCollector, display, logger, alerts

namespace mdfh::core {

// TODO: Implement in Pipeline::consumer_stage()
// - Loop while running_
// - Q3.try_pop() → StatsCollector::process()
// - Every 100ms: ConsoleDisplay::refresh()
// - Every 1s: StatsCollector::tick(), CsvLogger::write(), AlertMonitor::check()

}  // namespace mdfh::core
