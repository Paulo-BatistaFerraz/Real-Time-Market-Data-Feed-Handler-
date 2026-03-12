#include "qf/consumer/csv_logger.hpp"
#include <chrono>
#include <thread>
#include <iomanip>

namespace qf::consumer {

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
    file_ << "timestamp,total_updates,updates_per_sec,peak_rate,p50_us,p95_us,p99_us,p999_us,min_us,max_us,gaps\n";
    file_.flush();
}

void CsvLogger::write_row(const StatsSnapshot& snap) {
    auto now = std::chrono::system_clock::now();
    auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    file_ << epoch_ms << ","
           << snap.total_updates << ","
           << std::fixed << std::setprecision(1) << snap.msg_rate << ","
           << std::fixed << std::setprecision(1) << snap.peak_rate << ","
           << (snap.p50_ns / 1000) << ","
           << (snap.p95_ns / 1000) << ","
           << (snap.p99_ns / 1000) << ","
           << (snap.p999_ns / 1000) << ","
           << (snap.min_ns == UINT64_MAX ? 0 : snap.min_ns / 1000) << ","
           << (snap.max_ns / 1000) << ","
           << snap.gap_count << "\n";
    file_.flush();
}

}  // namespace qf::consumer
