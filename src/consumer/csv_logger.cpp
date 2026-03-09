#include "mdfh/consumer/csv_logger.hpp"
#include <chrono>
#include <thread>

namespace mdfh::consumer {

// TODO: Implement CSV logging logic

CsvLogger::CsvLogger(const std::string& path, const StatsCollector& collector, uint32_t interval_sec)
    : path_(path), collector_(collector), interval_sec_(interval_sec) {}

CsvLogger::~CsvLogger() {
    stop();
}

void CsvLogger::start() {
    file_.open(path_);
    if (!file_.is_open()) return;
    write_header();
    running_.store(true);
    thread_ = std::thread(&CsvLogger::run, this);
}

void CsvLogger::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
    if (file_.is_open()) file_.close();
}

void CsvLogger::run() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(interval_sec_));
        if (!running_.load()) break;
        auto snap = collector_.snapshot();
        write_row(snap);
    }
}

void CsvLogger::write_header() {
    file_ << "timestamp,msg_count,msg_rate,p50_us,p95_us,p99_us,p999_us,gaps\n";
    file_.flush();
}

void CsvLogger::write_row(const StatsSnapshot& snap) {
    (void)snap;
    // TODO: Write stats row
}

}  // namespace mdfh::consumer
