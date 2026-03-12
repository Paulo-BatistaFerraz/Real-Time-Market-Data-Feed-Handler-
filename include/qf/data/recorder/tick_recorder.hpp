#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <atomic>
#include "qf/data/data_types.hpp"
#include "qf/data/recorder/binary_writer.hpp"
#include "qf/core/pipeline_types.hpp"
#include "qf/core/event_bus.hpp"

namespace qf::data {

// TickRecorder — subscribes to pipeline events and records every message
// via BinaryWriter to a binary tick file.
//
// Usage:
//   TickRecorder recorder(event_bus);
//   recorder.start("ticks_2026-03-12.qfbt");
//   // ... pipeline runs, events flow ...
//   recorder.stop();  // flushes, closes, prints summary
class TickRecorder {
public:
    explicit TickRecorder(core::EventBus& bus);

    // Begin recording to the given file path.
    void start(const std::string& output_path);

    // Flush and close the file, print summary (records written, file size).
    void stop();

    // Whether we are currently recording.
    bool is_recording() const { return recording_.load(std::memory_order_relaxed); }

    // Number of records written so far.
    uint64_t records_written() const;

private:
    // Callback invoked by EventBus for each ParsedEvent.
    void on_parsed_event(const core::ParsedEvent& event);

    // Convert a ParsedMessage into a TickRecord for storage.
    TickRecord to_tick_record(const core::ParsedEvent& event) const;

    core::EventBus&                  bus_;
    std::unique_ptr<BinaryWriter>    writer_;
    std::atomic<bool>                recording_{false};
    std::string                      output_path_;
};

}  // namespace qf::data
