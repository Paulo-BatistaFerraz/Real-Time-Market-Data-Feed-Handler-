#include "qf/data/recorder/tick_recorder.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <variant>

namespace qf::data {

TickRecorder::TickRecorder(core::EventBus& bus) : bus_(bus) {
    // Subscribe to ParsedEvent from the pipeline.
    bus_.subscribe<core::ParsedEvent>(
        [this](const core::ParsedEvent& event) {
            on_parsed_event(event);
        });
}

void TickRecorder::start(const std::string& output_path) {
    if (recording_.load(std::memory_order_relaxed)) {
        throw std::runtime_error("TickRecorder: already recording");
    }

    output_path_ = output_path;
    writer_ = std::make_unique<BinaryWriter>(output_path_);
    recording_.store(true, std::memory_order_release);
}

void TickRecorder::stop() {
    if (!recording_.load(std::memory_order_acquire)) {
        return;
    }

    recording_.store(false, std::memory_order_release);

    uint64_t count = writer_->record_count();
    writer_->close();

    // Compute file size.
    FILE* f = std::fopen(output_path_.c_str(), "rb");
    uint64_t file_size = 0;
    if (f) {
        std::fseek(f, 0, SEEK_END);
        file_size = static_cast<uint64_t>(std::ftell(f));
        std::fclose(f);
    }

    std::printf("TickRecorder: stopped. Records written: %llu, File size: %llu bytes (%s)\n",
                static_cast<unsigned long long>(count),
                static_cast<unsigned long long>(file_size),
                output_path_.c_str());

    writer_.reset();
}

uint64_t TickRecorder::records_written() const {
    if (writer_) {
        return writer_->record_count();
    }
    return 0;
}

void TickRecorder::on_parsed_event(const core::ParsedEvent& event) {
    if (!recording_.load(std::memory_order_relaxed)) {
        return;
    }

    TickRecord record = to_tick_record(event);
    writer_->write(record);
}

TickRecord TickRecorder::to_tick_record(const core::ParsedEvent& event) const {
    TickRecord record;
    record.timestamp = event.receive_timestamp;

    std::visit([&record](const auto& msg) {
        using T = std::decay_t<decltype(msg)>;

        if constexpr (std::is_same_v<T, AddOrder>) {
            record.message_type = AddOrder::TYPE;
            record.symbol = msg.symbol;
            record.payload_length = sizeof(AddOrder);
            std::memcpy(record.payload, &msg, sizeof(AddOrder));
        } else if constexpr (std::is_same_v<T, CancelOrder>) {
            record.message_type = CancelOrder::TYPE;
            // CancelOrder has no symbol — leave default.
            record.payload_length = sizeof(CancelOrder);
            std::memcpy(record.payload, &msg, sizeof(CancelOrder));
        } else if constexpr (std::is_same_v<T, ExecuteOrder>) {
            record.message_type = ExecuteOrder::TYPE;
            record.payload_length = sizeof(ExecuteOrder);
            std::memcpy(record.payload, &msg, sizeof(ExecuteOrder));
        } else if constexpr (std::is_same_v<T, ReplaceOrder>) {
            record.message_type = ReplaceOrder::TYPE;
            record.payload_length = sizeof(ReplaceOrder);
            std::memcpy(record.payload, &msg, sizeof(ReplaceOrder));
        } else if constexpr (std::is_same_v<T, TradeMessage>) {
            record.message_type = TradeMessage::TYPE;
            record.symbol = msg.symbol;
            record.payload_length = sizeof(TradeMessage);
            std::memcpy(record.payload, &msg, sizeof(TradeMessage));
        }
    }, event.message);

    return record;
}

}  // namespace qf::data
