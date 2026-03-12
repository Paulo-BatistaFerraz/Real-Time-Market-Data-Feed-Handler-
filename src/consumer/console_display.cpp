#include "qf/consumer/console_display.hpp"
#include "qf/utils/format_helpers.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cstring>

namespace qf::consumer {

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
    clear_screen();

    std::cout << "================ QuantFlow Market Data Feed Handler ================\n\n";

    // Latency stats table
    std::cout << "--- Latency ---\n";
    std::cout << std::left << std::setw(12) << "  p50:"
              << std::right << std::setw(10) << utils::format_latency_ns(snap.p50_ns)
              << "    "
              << std::left << std::setw(12) << "p95:"
              << std::right << std::setw(10) << utils::format_latency_ns(snap.p95_ns) << "\n";
    std::cout << std::left << std::setw(12) << "  p99:"
              << std::right << std::setw(10) << utils::format_latency_ns(snap.p99_ns)
              << "    "
              << std::left << std::setw(12) << "p999:"
              << std::right << std::setw(10) << utils::format_latency_ns(snap.p999_ns) << "\n";
    std::cout << std::left << std::setw(12) << "  min:"
              << std::right << std::setw(10) << utils::format_latency_ns(snap.min_ns)
              << "    "
              << std::left << std::setw(12) << "max:"
              << std::right << std::setw(10) << utils::format_latency_ns(snap.max_ns) << "\n\n";

    // Throughput stats
    std::cout << "--- Throughput ---\n";
    std::cout << "  Updates/sec: " << utils::format_rate(snap.msg_rate)
              << "    Peak: " << utils::format_rate(snap.peak_rate)
              << "    Total: " << utils::format_number(snap.total_updates)
              << "    Gaps: " << snap.gap_count << "\n\n";

    // Top-of-book table
    auto books = books_.get_snapshot();
    if (!books.empty()) {
        std::cout << "--- Top of Book ---\n";
        std::cout << std::left << std::setw(10) << "  Symbol"
                  << std::right << std::setw(12) << "Bid Px"
                  << std::right << std::setw(10) << "Bid Qty"
                  << std::right << std::setw(12) << "Ask Px"
                  << std::right << std::setw(10) << "Ask Qty"
                  << "\n";
        std::cout << "  " << std::string(52, '-') << "\n";

        for (const auto& b : books) {
            std::string bid_px_str = "---";
            std::string bid_qty_str = "---";
            std::string ask_px_str = "---";
            std::string ask_qty_str = "---";

            if (b.best_bid.has_value()) {
                bid_px_str = utils::format_price(price_to_double(b.best_bid->price));
                bid_qty_str = std::to_string(b.best_bid->total_quantity);
            }
            if (b.best_ask.has_value()) {
                ask_px_str = utils::format_price(price_to_double(b.best_ask->price));
                ask_qty_str = std::to_string(b.best_ask->total_quantity);
            }

            std::cout << "  " << std::left << std::setw(8)
                      << std::string(b.symbol.data, strnlen(b.symbol.data, SYMBOL_LENGTH))
                      << std::right << std::setw(12) << bid_px_str
                      << std::right << std::setw(10) << bid_qty_str
                      << std::right << std::setw(12) << ask_px_str
                      << std::right << std::setw(10) << ask_qty_str
                      << "\n";
        }
    }

    std::cout << "\n====================================================================\n";
    std::cout.flush();
}

void ConsoleDisplay::clear_screen() {
    std::cout << "\033[2J\033[H";
}

}  // namespace qf::consumer
