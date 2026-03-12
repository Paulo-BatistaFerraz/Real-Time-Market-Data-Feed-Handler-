#include "qf/core/pipeline.hpp"
#include "qf/consumer/stats_collector.hpp"
#include "qf/consumer/console_display.hpp"
#include "qf/consumer/csv_logger.hpp"

#include <chrono>

namespace qf::core {

void Pipeline::consumer_stage() {
    // Start console display (runs its own refresh thread)
    consumer::ConsoleDisplay display(stats_collector_, book_manager_);
    if (config_.enable_display) {
        display.start();
    }

    // Optionally start CSV logger
    std::unique_ptr<consumer::CsvLogger> csv_logger;
    if (config_.enable_csv) {
        csv_logger = std::make_unique<consumer::CsvLogger>(config_.csv_path, stats_collector_);
        csv_logger->start();
    }

    auto last_tick = std::chrono::steady_clock::now();
    BookUpdate update;

    while (running_.load(std::memory_order_acquire)) {
        if (q3_.try_pop(update)) {
            stats_collector_.process(update);
        } else {
            std::this_thread::yield();
        }

        // Tick the stats collector once per second
        auto now = std::chrono::steady_clock::now();
        if (now - last_tick >= std::chrono::seconds(1)) {
            stats_collector_.tick();
            last_tick = now;
        }
    }

    // Clean shutdown
    if (config_.enable_display) {
        display.stop();
    }
    if (csv_logger) {
        csv_logger->stop();
    }
}

}  // namespace qf::core
