#include "mdfh/core/pipeline.hpp"
#include "mdfh/common/clock.hpp"

// Thread 3: Order Book Engine
// Pops ParsedEvents from Q2, applies to BookManager, pushes BookUpdates to Q3

namespace mdfh::core {

// TODO: Implement in Pipeline::book_stage()
// - Loop while running_
// - Q2.try_pop() → BookManager::process() → BookUpdate
// - Set latency_ns = Clock::now_ns() - recv_timestamp
// - Q3.try_push()

}  // namespace mdfh::core
