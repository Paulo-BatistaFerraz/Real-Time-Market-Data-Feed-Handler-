#include "qf/network/tcp_client.hpp"
#include "qf/common/logging.hpp"

namespace qf::network {

TcpClient::TcpClient(asio::io_context& io, const SocketOptions& opts)
    : io_(io)
    , socket_(io)
    , resolver_(io)
    , reconnect_timer_(io)
    , opts_(opts)
{
}

void TcpClient::connect(const std::string& host, uint16_t port) {
    host_ = host;
    port_ = port;
    running_ = true;
    do_connect();
}

void TcpClient::disconnect() {
    running_ = false;
    reconnect_timer_.cancel();
    do_close();
}

void TcpClient::send(const uint8_t* data, size_t len) {
    if (!connected_.load(std::memory_order_relaxed)) return;

    // Copy data into a shared buffer for the async write
    auto buf = std::make_shared<std::vector<uint8_t>>(data, data + len);

    asio::async_write(
        socket_, asio::buffer(*buf),
        [this, buf](std::error_code ec, size_t bytes_transferred) {
            if (!ec) {
                bytes_sent_ += bytes_transferred;
            } else {
                QF_LOG_WARN_T("TCP", "client write error: {}", ec.message());
                do_close();
                schedule_reconnect();
            }
        });
}

std::string TcpClient::remote_address() const {
    try {
        return socket_.remote_endpoint().address().to_string();
    } catch (...) {
        return "";
    }
}

uint16_t TcpClient::remote_port() const {
    try {
        return socket_.remote_endpoint().port();
    } catch (...) {
        return 0;
    }
}

void TcpClient::do_connect() {
    if (!running_) return;

    QF_LOG_INFO_T("TCP", "client connecting to {}:{}...", host_, port_);

    resolver_.async_resolve(
        host_, std::to_string(port_),
        [this](std::error_code ec, asio::ip::tcp::resolver::results_type results) {
            if (ec) {
                QF_LOG_ERROR_T("TCP", "resolve failed for {}:{}: {}",
                               host_, port_, ec.message());
                schedule_reconnect();
                return;
            }

            asio::async_connect(
                socket_, results,
                [this](std::error_code ec, const asio::ip::tcp::endpoint& endpoint) {
                    if (!ec) {
                        apply_socket_options();
                        connected_.store(true, std::memory_order_relaxed);

                        QF_LOG_INFO_T("TCP", "client connected to {}:{}",
                                      endpoint.address().to_string(),
                                      endpoint.port());

                        if (on_connect_) {
                            on_connect_();
                        }

                        do_read();
                    } else {
                        QF_LOG_WARN_T("TCP", "connect to {}:{} failed: {}",
                                      host_, port_, ec.message());
                        schedule_reconnect();
                    }
                });
        });
}

void TcpClient::do_read() {
    if (!connected_.load(std::memory_order_relaxed)) return;

    socket_.async_read_some(
        asio::buffer(recv_buffer_, sizeof(recv_buffer_)),
        [this](std::error_code ec, size_t bytes_transferred) {
            if (!ec && bytes_transferred > 0) {
                bytes_received_ += bytes_transferred;

                if (on_data_) {
                    on_data_(recv_buffer_, bytes_transferred);
                }

                // Continue the read loop
                if (connected_.load(std::memory_order_relaxed)) {
                    do_read();
                }
            } else {
                // EOF or error — connection lost
                if (ec != asio::error::operation_aborted) {
                    QF_LOG_DEBUG_T("TCP", "client read ended: {}", ec.message());
                }
                do_close();
                schedule_reconnect();
            }
        });
}

void TcpClient::do_close() {
    if (!socket_.is_open()) return;

    bool was_connected = connected_.exchange(false, std::memory_order_relaxed);

    std::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    if (was_connected && on_disconnect_) {
        on_disconnect_();
    }
}

void TcpClient::schedule_reconnect() {
    if (!running_ || reconnect_interval_ms_ == 0) return;

    QF_LOG_INFO_T("TCP", "client reconnecting in {} ms...", reconnect_interval_ms_);

    reconnect_timer_.expires_after(
        std::chrono::milliseconds(reconnect_interval_ms_));

    reconnect_timer_.async_wait(
        [this](std::error_code ec) {
            if (!ec && running_) {
                // Re-open the socket for the next connect attempt
                socket_ = asio::ip::tcp::socket(io_);
                do_connect();
            }
        });
}

void TcpClient::apply_socket_options() {
    socket_.set_option(asio::socket_base::receive_buffer_size(opts_.recv_buf_size));
    socket_.set_option(asio::socket_base::send_buffer_size(opts_.send_buf_size));
    socket_.set_option(asio::ip::tcp::no_delay(true));
}

}  // namespace qf::network
