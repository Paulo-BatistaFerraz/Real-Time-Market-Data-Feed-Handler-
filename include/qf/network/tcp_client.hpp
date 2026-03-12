#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>
#include <atomic>
#include <asio.hpp>
#include <asio/steady_timer.hpp>
#include "qf/network/socket_config.hpp"

namespace qf::network {

// Callbacks for client events
using ClientOnConnect    = std::function<void()>;
using ClientOnDisconnect = std::function<void()>;
using ClientOnData       = std::function<void(const uint8_t*, size_t)>;

// ─────────────────────────────────────────────────────────────
//  TcpClient — async TCP client with auto-reconnect
// ─────────────────────────────────────────────────────────────

class TcpClient {
public:
    TcpClient(asio::io_context& io, const SocketOptions& opts = {});

    // Connect to remote host (async)
    void connect(const std::string& host, uint16_t port);

    // Graceful disconnect (stops reconnect timer)
    void disconnect();

    // Async write
    void send(const uint8_t* data, size_t len);

    bool is_connected() const { return connected_.load(std::memory_order_relaxed); }

    // Auto-reconnect configuration (0 = disabled)
    void set_reconnect_interval_ms(uint32_t ms) { reconnect_interval_ms_ = ms; }
    uint32_t reconnect_interval_ms() const       { return reconnect_interval_ms_; }

    // Register callbacks
    void set_on_connect(ClientOnConnect cb)       { on_connect_    = std::move(cb); }
    void set_on_disconnect(ClientOnDisconnect cb) { on_disconnect_ = std::move(cb); }
    void set_on_data(ClientOnData cb)             { on_data_       = std::move(cb); }

    uint64_t bytes_received() const { return bytes_received_; }
    uint64_t bytes_sent()     const { return bytes_sent_; }

    // Remote endpoint info (valid while connected)
    std::string remote_address() const;
    uint16_t    remote_port()    const;

private:
    void do_connect();
    void do_read();
    void do_close();
    void schedule_reconnect();
    void apply_socket_options();

    asio::io_context& io_;
    asio::ip::tcp::socket socket_;
    asio::ip::tcp::resolver resolver_;
    asio::steady_timer reconnect_timer_;
    SocketOptions opts_;

    std::string host_;
    uint16_t    port_ = 0;

    std::atomic<bool> connected_{false};
    bool running_    = false;    // user intent: should we be connected?
    uint32_t reconnect_interval_ms_ = 3000;  // default 3s, 0 = disabled

    ClientOnConnect    on_connect_;
    ClientOnDisconnect on_disconnect_;
    ClientOnData       on_data_;

    uint8_t recv_buffer_[8192];
    uint64_t bytes_received_ = 0;
    uint64_t bytes_sent_     = 0;
};

}  // namespace qf::network
