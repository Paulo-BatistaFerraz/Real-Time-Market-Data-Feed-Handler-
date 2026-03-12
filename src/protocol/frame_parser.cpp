#include "qf/protocol/frame_parser.hpp"
#include <cstring>

namespace qf::protocol {

FrameParseResult FrameParser::parse(const uint8_t* datagram, size_t len) {
    FrameParseResult result{{}, {}, ErrorCode::Ok};

    if (!datagram || len == 0) return result;

    if (len < sizeof(BatchHeader)) {
        result.error = ErrorCode::BufferTooSmall;
        return result;
    }

    std::memcpy(&result.batch_header, datagram, sizeof(BatchHeader));

    const uint8_t* msg_data = datagram + sizeof(BatchHeader);
    size_t msg_len = len - sizeof(BatchHeader);

    parse(msg_data, msg_len, [&](const qf::MessageHeader& header, const uint8_t* payload) {
        const uint8_t* msg_start = payload - sizeof(qf::MessageHeader);
        result.messages.push_back({msg_start, header.message_length});
    });

    return result;
}

std::vector<MessageView> FrameParser::parse_raw(const uint8_t* data, size_t len) {
    std::vector<MessageView> views;

    parse(data, len, [&](const qf::MessageHeader& header, const uint8_t* payload) {
        const uint8_t* msg_start = payload - sizeof(qf::MessageHeader);
        views.push_back({msg_start, header.message_length});
    });

    return views;
}

}  // namespace qf::protocol
