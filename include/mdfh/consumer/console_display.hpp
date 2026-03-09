#pragma once

#include <atomic>
#include <thread>
#include "mdfh/consumer/stats_collector.hpp"
#include "mdfh/core/book_manager.hpp"
#include "mdfh/common/constants.hpp"

namespace mdfh::consumer {

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

}  // namespace mdfh::consumer
