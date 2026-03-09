#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <string>
#include "mdfh/common/constants.hpp"
#include "mdfh/core/spsc_queue.hpp"
#include "mdfh/core/pipeline_types.hpp"
#include "mdfh/core/book_manager.hpp"

namespace mdfh::core {

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

private:
    PipelineConfig config_;
    std::atomic<bool> running_{false};

    // The 3 queues connecting 4 threads
    SPSCQueue<RawPacket,    Q1_CAPACITY> q1_;  // Network → Parser
    SPSCQueue<ParsedEvent,  Q2_CAPACITY> q2_;  // Parser → Book
    SPSCQueue<BookUpdate,   Q3_CAPACITY> q3_;  // Book → Consumer

    // Shared state
    BookManager book_manager_;
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

}  // namespace mdfh::core
