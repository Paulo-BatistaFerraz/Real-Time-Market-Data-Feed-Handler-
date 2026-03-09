#include "mdfh/consumer/console_display.hpp"
#include "mdfh/utils/format_helpers.hpp"
#include <iostream>
#include <chrono>
#include <thread>

namespace mdfh::consumer {

// TODO: Implement console display logic

ConsoleDisplay::ConsoleDisplay(const StatsCollector& collector, const core::BookManager& books)
    : collector_(collector), books_(books) {}

void ConsoleDisplay::start() {
    running_.store(true);
    thread_ = std::thread(&ConsoleDisplay::run, this);
}

void ConsoleDisplay::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
}

void ConsoleDisplay::run() {
    while (running_.load()) {
        auto snap = collector_.snapshot();
        render(snap);
        std::this_thread::sleep_for(std::chrono::milliseconds(DISPLAY_REFRESH_MS));
    }
}

void ConsoleDisplay::render(const StatsSnapshot& snap) {
    (void)snap;
    // TODO: ANSI escape codes for in-place terminal rendering
}

void ConsoleDisplay::clear_screen() {
    std::cout << "\033[2J\033[H";
}

}  // namespace mdfh::consumer
