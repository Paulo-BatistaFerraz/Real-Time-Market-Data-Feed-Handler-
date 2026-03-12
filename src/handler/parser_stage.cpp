#include "qf/core/pipeline.hpp"
#include "qf/protocol/frame_parser.hpp"
#include "qf/protocol/validator.hpp"
#include "qf/protocol/encoder.hpp"

namespace qf::core {

void Pipeline::parser_stage() {
    protocol::Validator validator;
    RawPacket pkt;

    while (running_.load(std::memory_order_acquire)) {
        if (!q1_.try_pop(pkt)) {
            // Yield to avoid busy-spinning when queue is empty
            std::this_thread::yield();
            continue;
        }

        // Parse the datagram (with BatchHeader) into individual messages
        auto result = protocol::FrameParser::parse(pkt.data, pkt.length);

        if (!result.ok()) {
            ++stats_.parse_errors;
            continue;
        }

        for (const auto& view : result.messages) {
            // Validate the raw message bytes
            auto vr = protocol::Validator::validate(view.data, view.len);
            if (!vr.ok()) {
                ++stats_.parse_errors;
                continue;
            }

            // Decode into a ParsedMessage variant
            ParsedEvent event;
            event.message = protocol::Encoder::parse(view.data);
            event.receive_timestamp = pkt.receive_timestamp;

            if (!q2_.try_push(std::move(event))) {
                ++stats_.queue_drops;
            } else {
                ++stats_.messages_parsed;
            }
        }
    }
}

}  // namespace qf::core
