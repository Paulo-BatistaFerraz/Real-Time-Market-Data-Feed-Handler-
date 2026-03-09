#include "mdfh/core/pipeline.hpp"
#include "mdfh/network/multicast_receiver.hpp"
#include "mdfh/common/clock.hpp"

// Thread 1: Network Receiver
// Receives UDP multicast packets, timestamps them, pushes to Q1

namespace mdfh::core {

// TODO: Implement in Pipeline::network_stage()
// - Create MulticastReceiver with callback
// - In callback: copy data + timestamp into RawPacket, try_push to Q1
// - Run io_context until running_ == false

}  // namespace mdfh::core
