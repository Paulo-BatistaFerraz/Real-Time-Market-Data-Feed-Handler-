#pragma once

#include <atomic>
#include <thread>
#include "qf/consumer/stats_collector.hpp"
#include "qf/core/book_manager.hpp"
#include "qf/common/constants.hpp"

namespace qf::consumer {

class ConsoleDisplay {
public:
    ConsoleDisplay(const StatsCollector& collector, const core::BookManager& books);

    // Start display refresh thread (100ms interval)
    void start();

    // Stop and join thread
    void stop();

private:
    const StatsCollector& collector_;
    const core::BookManager& books_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    void run();
    void render(const StatsSnapshot& snap);
    void clear_screen();
};

}  // namespace qf::consumer
