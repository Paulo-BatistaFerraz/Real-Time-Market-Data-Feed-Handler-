#include "qf/network/multicast_receiver.hpp"
#include "qf/common/clock.hpp"

namespace qf::network {

MulticastReceiver::MulticastReceiver(asio::io_context& io, const std::string& group_ip,
                                     uint16_t port, ReceiveCallback callback,
                                     const SocketOptions& opts)
    : socket_(io, asio::ip::udp::v4())
    , callback_(std::move(callback))
{
    auto listen_addr = asio::ip::make_address_v4(opts.interface_addr);

    // SO_REUSEADDR — allows multiple receivers on the same port
    if (opts.reuse_addr) {
        socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    }

    // Receive buffer size
    socket_.set_option(asio::socket_base::receive_buffer_size(opts.recv_buf_size));

    // Multicast loopback
    socket_.set_option(asio::ip::multicast::enable_loopback(opts.loopback));

    // Bind to port on the specified interface
    socket_.bind(asio::ip::udp::endpoint(listen_addr, port));

    // Join the multicast group on the specified interface
    socket_.set_option(asio::ip::multicast::join_group(
        asio::ip::make_address_v4(group_ip), listen_addr));
}

void MulticastReceiver::start() {
    running_ = true;
    do_receive();
}

void MulticastReceiver::stop() {
    running_ = false;
    socket_.cancel();
}

void MulticastReceiver::do_receive() {
    if (!running_) return;

    socket_.async_receive_from(
        asio::buffer(recv_buffer_, RAW_PACKET_BUF), sender_endpoint_,
        [this](std::error_code ec, size_t bytes_transferred) {
            if (!ec && bytes_transferred > 0) {
                // Timestamp immediately on arrival
                uint64_t ts = qf::Clock::now_ns();

                bytes_received_ += bytes_transferred;
                ++packets_received_;

                callback_(recv_buffer_, bytes_transferred, ts);
            }

            // Continue the receive loop
            if (running_) {
                do_receive();
            }
        });
}

}  // namespace qf::network
