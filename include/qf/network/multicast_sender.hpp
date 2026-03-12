#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <asio.hpp>
#include "qf/network/socket_config.hpp"

namespace qf::network {

class MulticastSender {
public:
    MulticastSender(asio::io_context& io, const std::string& group_ip, uint16_t port,
                    const SocketOptions& opts = {});

    // Send raw bytes over multicast
    void send(const uint8_t* data, size_t len);

    // Async send with completion handler
    void async_send(const uint8_t* data, size_t len,
                    std::function<void(std::error_code, size_t)> handler);

    uint64_t bytes_sent() const { return bytes_sent_; }
    uint64_t packets_sent() const { return packets_sent_; }

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint endpoint_;
    uint64_t bytes_sent_ = 0;
    uint64_t packets_sent_ = 0;
};

}  // namespace qf::network
