#include "mdfh/protocol/frame_parser.hpp"

namespace mdfh::protocol {

// TODO: Implement frame parsing logic

FrameParseResult FrameParser::parse(const uint8_t* datagram, size_t len) {
    (void)datagram; (void)len;
    return {{}, {}, ErrorCode::Ok};
}

std::vector<MessageView> FrameParser::parse_raw(const uint8_t* data, size_t len) {
    (void)data; (void)len;
    return {};
}

}  // namespace mdfh::protocol
