#include "qf/network/multicast_sender.hpp"

namespace qf::network {

MulticastSender::MulticastSender(asio::io_context& io, const std::string& group_ip,
                                 uint16_t port, const SocketOptions& opts)
    : socket_(io, asio::ip::udp::v4())
    , endpoint_(asio::ip::make_address(group_ip), port)
{
    // SO_REUSEADDR
    if (opts.reuse_addr) {
        socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    }

    // Send buffer size
    socket_.set_option(asio::socket_base::send_buffer_size(opts.send_buf_size));

    // Multicast TTL
    socket_.set_option(asio::ip::multicast::hops(opts.multicast_ttl));

    // Multicast loopback (receive own packets on this host)
    socket_.set_option(asio::ip::multicast::enable_loopback(opts.loopback));

    // Outgoing interface for multicast packets
    socket_.set_option(asio::ip::multicast::outbound_interface(
        asio::ip::make_address_v4(opts.interface_addr)));
}

void MulticastSender::send(const uint8_t* data, size_t len) {
    auto bytes = socket_.send_to(asio::buffer(data, len), endpoint_);
    bytes_sent_ += bytes;
    ++packets_sent_;
}

void MulticastSender::async_send(const uint8_t* data, size_t len,
                                 std::function<void(std::error_code, size_t)> handler) {
    socket_.async_send_to(
        asio::buffer(data, len), endpoint_,
        [this, handler = std::move(handler)](std::error_code ec, size_t bytes) {
            if (!ec) {
                bytes_sent_ += bytes;
                ++packets_sent_;
            }
            if (handler) handler(ec, bytes);
        });
}

}  // namespace qf::network
