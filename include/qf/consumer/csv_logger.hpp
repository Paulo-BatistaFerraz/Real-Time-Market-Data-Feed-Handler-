#pragma once

#include <string>
#include <fstream>
#include <atomic>
#include <thread>
#include "qf/consumer/stats_collector.hpp"
#include "qf/common/constants.hpp"

namespace qf::consumer {

class CsvLogger {
public:
    CsvLogger(const std::string& path, const StatsCollector& collector,
              uint32_t interval_sec = CSV_LOG_INTERVAL_SEC);
    ~CsvLogger();

    void start();
    void stop();

private:
    std::string path_;
    const StatsCollector& collector_;
    uint32_t interval_sec_;
    std::ofstream file_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    void run();
    void write_header();
    void write_row(const StatsSnapshot& snap);
};

}  // namespace qf::consumer
