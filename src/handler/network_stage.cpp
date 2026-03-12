#include "qf/core/pipeline.hpp"
#include "qf/network/multicast_receiver.hpp"
#include "qf/common/clock.hpp"

#include <cstring>

namespace qf::core {

void Pipeline::network_stage() {
    asio::io_context io;

    network::MulticastReceiver receiver(
        io, config_.multicast_group, config_.port,
        [this](const uint8_t* data, size_t length, uint64_t receive_ts) {
            RawPacket pkt;
            pkt.length = (length <= sizeof(pkt.data)) ? length : sizeof(pkt.data);
            std::memcpy(pkt.data, data, pkt.length);
            pkt.receive_timestamp = receive_ts;

            if (!q1_.try_push(std::move(pkt))) {
                ++stats_.queue_drops;
            } else {
                ++stats_.packets_received;
            }
        }
    );

    receiver.start();

    // Run io_context in short bursts so we can check the stop flag
    while (running_.load(std::memory_order_acquire)) {
        io.run_for(std::chrono::milliseconds(10));
    }

    receiver.stop();
}

}  // namespace qf::core
