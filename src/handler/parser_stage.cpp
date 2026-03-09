#include "mdfh/core/pipeline.hpp"
#include "mdfh/protocol/frame_parser.hpp"
#include "mdfh/protocol/validator.hpp"
#include "mdfh/protocol/encoder.hpp"

// Thread 2: Protocol Parser
// Pops RawPackets from Q1, splits into messages, decodes, pushes to Q2

namespace mdfh::core {

// TODO: Implement in Pipeline::parser_stage()
// - Loop while running_
// - Q1.try_pop() → FrameParser::parse() → for each message:
//   - Validator::validate()
//   - Encoder::parse() → ParsedEvent
//   - Q2.try_push()
// - Count parse_errors and queue_drops

}  // namespace mdfh::core
