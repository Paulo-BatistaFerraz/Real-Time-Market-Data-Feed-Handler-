#include "mdfh/core/pipeline.hpp"
#include <iostream>

namespace mdfh::core {

// TODO: Implement pipeline logic

Pipeline::Pipeline(const PipelineConfig& config) : config_(config) {}

Pipeline::~Pipeline() {
    if (running_) stop();
}

void Pipeline::start() {
    running_.store(true, std::memory_order_release);
    network_thread_  = std::thread(&Pipeline::network_stage, this);
    parser_thread_   = std::thread(&Pipeline::parser_stage, this);
    book_thread_     = std::thread(&Pipeline::book_stage, this);
    consumer_thread_ = std::thread(&Pipeline::consumer_stage, this);
    std::cout << "[FEED] Pipeline started (4 threads)\n";
}

void Pipeline::stop() {
    running_.store(false, std::memory_order_release);
    if (network_thread_.joinable())  network_thread_.join();
    if (parser_thread_.joinable())   parser_thread_.join();
    if (book_thread_.joinable())     book_thread_.join();
    if (consumer_thread_.joinable()) consumer_thread_.join();
    std::cout << "[FEED] Pipeline stopped\n";
}

void Pipeline::network_stage() {
    // TODO: MulticastReceiver → RawPacket → Q1
}

void Pipeline::parser_stage() {
    // TODO: Q1 → FrameParser → Validator → Encoder::parse → Q2
}

void Pipeline::book_stage() {
    // TODO: Q2 → BookManager::process → Q3
}

void Pipeline::consumer_stage() {
    // TODO: Q3 → StatsCollector → ConsoleDisplay / CsvLogger / AlertMonitor
}

}  // namespace mdfh::core
