#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>
#include <asio.hpp>
#include "mdfh/common/constants.hpp"
#include "mdfh/network/socket_config.hpp"

namespace mdfh::network {

// Callback: (data, length, receive_timestamp_ns)
using ReceiveCallback = std::function<void(const uint8_t*, size_t, uint64_t)>;

class MulticastReceiver {
public:
    MulticastReceiver(asio::io_context& io, const std::string& group_ip, uint16_t port,
                      ReceiveCallback callback, const SocketOptions& opts = {});

    // Start async receive loop
    void start();

    // Stop receiving
    void stop();

    uint64_t bytes_received() const { return bytes_received_; }
    uint64_t packets_received() const { return packets_received_; }

private:
    void do_receive();

    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint sender_endpoint_;
    ReceiveCallback callback_;
    uint8_t recv_buffer_[RAW_PACKET_BUF];
    uint64_t bytes_received_ = 0;
    uint64_t packets_received_ = 0;
    bool running_ = false;
};

}  // namespace mdfh::network
