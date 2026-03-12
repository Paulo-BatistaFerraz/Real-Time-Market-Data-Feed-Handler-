#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <asio.hpp>
#include "qf/network/socket_config.hpp"

namespace qf::network {

// Forward declaration
class TcpSession;

// Session identifier — monotonically increasing, never reused
using SessionId = uint64_t;

// Callbacks
using OnConnect    = std::function<void(SessionId)>;
using OnDisconnect = std::function<void(SessionId)>;
using OnData       = std::function<void(SessionId, const uint8_t*, size_t)>;

// ─────────────────────────────────────────────────────────────
//  TcpSession — one accepted client connection
// ─────────────────────────────────────────────────────────────

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(asio::ip::tcp::socket socket, SessionId id,
               OnData on_data, OnDisconnect on_disconnect);

    // Start the async read loop
    void start();

    // Graceful shutdown
    void stop();

    // Async write
    void send(const uint8_t* data, size_t len);

    SessionId id() const { return id_; }
    bool      is_open() const { return socket_.is_open(); }

    uint64_t bytes_received() const { return bytes_received_; }
    uint64_t bytes_sent()     const { return bytes_sent_; }

    // Remote endpoint info (valid while socket is open)
    std::string remote_address() const;
    uint16_t    remote_port()    const;

private:
    void do_read();
    void do_close();

    asio::ip::tcp::socket socket_;
    SessionId id_;
    OnData on_data_;
    OnDisconnect on_disconnect_;
    uint8_t recv_buffer_[8192];
    uint64_t bytes_received_ = 0;
    uint64_t bytes_sent_     = 0;
    bool running_ = false;
};

// ─────────────────────────────────────────────────────────────
//  TcpServer — accepts connections, manages active sessions
// ─────────────────────────────────────────────────────────────

class TcpServer {
public:
    TcpServer(asio::io_context& io, const SocketOptions& opts = {});

    // Start listening and accepting connections on the given port
    void start(uint16_t port);

    // Stop the server and close all sessions
    void stop();

    // Send to a specific session
    void send(SessionId id, const uint8_t* data, size_t len);

    // Send to all active sessions
    void broadcast(const uint8_t* data, size_t len);

    // Disconnect a specific session
    void disconnect(SessionId id);

    // Register callbacks
    void set_on_connect(OnConnect cb)       { on_connect_    = std::move(cb); }
    void set_on_disconnect(OnDisconnect cb) { on_disconnect_ = std::move(cb); }
    void set_on_data(OnData cb)             { on_data_       = std::move(cb); }

    size_t active_session_count() const;

private:
    void do_accept();
    void remove_session(SessionId id);

    asio::io_context& io_;
    asio::ip::tcp::acceptor acceptor_;
    SocketOptions opts_;

    std::atomic<SessionId> next_id_{1};
    mutable std::mutex sessions_mutex_;
    std::unordered_map<SessionId, std::shared_ptr<TcpSession>> sessions_;

    OnConnect    on_connect_;
    OnDisconnect on_disconnect_;
    OnData       on_data_;

    bool running_ = false;
};

}  // namespace qf::network
