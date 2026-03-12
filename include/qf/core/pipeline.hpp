#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <string>
#include "qf/common/constants.hpp"
#include "qf/core/spsc_queue.hpp"
#include "qf/core/pipeline_types.hpp"
#include "qf/core/book_manager.hpp"
#include "qf/consumer/stats_collector.hpp"

namespace qf::core {

struct PipelineConfig {
    std::string multicast_group = MULTICAST_GROUP;
    uint16_t    port            = MULTICAST_PORT;
    bool        enable_display  = true;
    bool        enable_csv      = false;
    std::string csv_path        = "stats.csv";
};

struct PipelineStats {
    uint64_t packets_received = 0;
    uint64_t messages_parsed  = 0;
    uint64_t book_updates     = 0;
    uint64_t parse_errors     = 0;
    uint64_t queue_drops      = 0;
};

class Pipeline {
public:
    explicit Pipeline(const PipelineConfig& config);
    ~Pipeline();

    // Start all 4 threads
    void start();

    // Signal stop and join all threads
    void stop();

    bool is_running() const { return running_.load(std::memory_order_acquire); }

    const PipelineStats& stats() const { return stats_; }
    const BookManager& books() const { return book_manager_; }

    // Return current performance stats snapshot
    consumer::StatsSnapshot get_stats() const;

private:
    PipelineConfig config_;
    std::atomic<bool> running_{false};

    // The 3 queues connecting 4 threads
    SPSCQueue<RawPacket,    Q1_CAPACITY> q1_;  // Network → Parser
    SPSCQueue<ParsedEvent,  Q2_CAPACITY> q2_;  // Parser → Book
    SPSCQueue<BookUpdate,   Q3_CAPACITY> q3_;  // Book → Consumer

    // Shared state
    BookManager book_manager_;
    consumer::StatsCollector stats_collector_;
    PipelineStats stats_;

    // The 4 threads
    std::thread network_thread_;
    std::thread parser_thread_;
    std::thread book_thread_;
    std::thread consumer_thread_;

    // Stage functions (each runs in its own thread)
    void network_stage();
    void parser_stage();
    void book_stage();
    void consumer_stage();
};

}  // namespace qf::core
