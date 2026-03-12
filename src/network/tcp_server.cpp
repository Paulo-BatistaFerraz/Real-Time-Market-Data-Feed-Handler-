#include "qf/network/tcp_server.hpp"
#include "qf/common/logging.hpp"

namespace qf::network {

// ═════════════════════════════════════════════════════════════
//  TcpSession
// ═════════════════════════════════════════════════════════════

TcpSession::TcpSession(asio::ip::tcp::socket socket, SessionId id,
                       OnData on_data, OnDisconnect on_disconnect)
    : socket_(std::move(socket))
    , id_(id)
    , on_data_(std::move(on_data))
    , on_disconnect_(std::move(on_disconnect))
{
}

void TcpSession::start() {
    running_ = true;
    do_read();
}

void TcpSession::stop() {
    running_ = false;
    do_close();
}

void TcpSession::send(const uint8_t* data, size_t len) {
    if (!socket_.is_open()) return;

    // Copy data into a shared buffer for the async write
    auto buf = std::make_shared<std::vector<uint8_t>>(data, data + len);
    auto self = shared_from_this();

    asio::async_write(
        socket_, asio::buffer(*buf),
        [self, buf](std::error_code ec, size_t bytes_transferred) {
            if (!ec) {
                self->bytes_sent_ += bytes_transferred;
            } else {
                QF_LOG_WARN_T("TCP", "session {} write error: {}",
                              self->id_, ec.message());
                self->do_close();
            }
        });
}

std::string TcpSession::remote_address() const {
    try {
        return socket_.remote_endpoint().address().to_string();
    } catch (...) {
        return "";
    }
}

uint16_t TcpSession::remote_port() const {
    try {
        return socket_.remote_endpoint().port();
    } catch (...) {
        return 0;
    }
}

void TcpSession::do_read() {
    if (!running_) return;

    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(recv_buffer_, sizeof(recv_buffer_)),
        [self](std::error_code ec, size_t bytes_transferred) {
            if (!ec && bytes_transferred > 0) {
                self->bytes_received_ += bytes_transferred;

                if (self->on_data_) {
                    self->on_data_(self->id_, self->recv_buffer_, bytes_transferred);
                }

                // Continue the read loop
                if (self->running_) {
                    self->do_read();
                }
            } else {
                // EOF or error — session is done
                if (ec != asio::error::operation_aborted) {
                    QF_LOG_DEBUG_T("TCP", "session {} read ended: {}",
                                   self->id_, ec.message());
                }
                self->do_close();
            }
        });
}

void TcpSession::do_close() {
    if (!socket_.is_open()) return;

    std::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    running_ = false;
    if (on_disconnect_) {
        on_disconnect_(id_);
    }
}

// ═════════════════════════════════════════════════════════════
//  TcpServer
// ═════════════════════════════════════════════════════════════

TcpServer::TcpServer(asio::io_context& io, const SocketOptions& opts)
    : io_(io)
    , acceptor_(io)
    , opts_(opts)
{
}

void TcpServer::start(uint16_t port) {
    asio::ip::tcp::endpoint endpoint(
        asio::ip::make_address(opts_.interface_addr), port);

    acceptor_.open(endpoint.protocol());

    // SO_REUSEADDR
    if (opts_.reuse_addr) {
        acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    }

    acceptor_.bind(endpoint);
    acceptor_.listen(asio::socket_base::max_listen_connections);

    running_ = true;

    QF_LOG_INFO_T("TCP", "server listening on {}:{}",
                  opts_.interface_addr, port);

    do_accept();
}

void TcpServer::stop() {
    running_ = false;

    std::error_code ec;
    acceptor_.close(ec);

    // Close all active sessions
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        session->stop();
    }
    sessions_.clear();

    QF_LOG_INFO_T("TCP", "server stopped");
}

void TcpServer::send(SessionId id, const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        it->second->send(data, len);
    }
}

void TcpServer::broadcast(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        session->send(data, len);
    }
}

void TcpServer::disconnect(SessionId id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        it->second->stop();
        sessions_.erase(it);
    }
}

size_t TcpServer::active_session_count() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

void TcpServer::do_accept() {
    if (!running_) return;

    acceptor_.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                SessionId id = next_id_.fetch_add(1, std::memory_order_relaxed);

                // Apply socket options
                socket.set_option(asio::socket_base::receive_buffer_size(opts_.recv_buf_size));
                socket.set_option(asio::socket_base::send_buffer_size(opts_.send_buf_size));
                socket.set_option(asio::ip::tcp::no_delay(true));

                QF_LOG_INFO_T("TCP", "accepted session {} from {}:{}",
                              id,
                              socket.remote_endpoint().address().to_string(),
                              socket.remote_endpoint().port());

                // Create session with a disconnect handler that cleans up our map
                auto session = std::make_shared<TcpSession>(
                    std::move(socket), id,
                    on_data_,
                    [this](SessionId sid) {
                        remove_session(sid);
                        if (on_disconnect_) {
                            on_disconnect_(sid);
                        }
                    });

                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_[id] = session;
                }

                session->start();

                if (on_connect_) {
                    on_connect_(id);
                }
            } else if (ec != asio::error::operation_aborted) {
                QF_LOG_ERROR_T("TCP", "accept error: {}", ec.message());
            }

            // Continue accepting
            if (running_) {
                do_accept();
            }
        });
}

void TcpServer::remove_session(SessionId id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(id);
}

}  // namespace qf::network
