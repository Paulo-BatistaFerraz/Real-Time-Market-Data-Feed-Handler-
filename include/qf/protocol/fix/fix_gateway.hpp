#pragma once

#include "qf/protocol/fix/fix_session.hpp"
#include "qf/protocol/fix/fix_message.hpp"
#include "qf/protocol/fix/fix_fields.hpp"
#include "qf/common/types.hpp"
#include "qf/common/clock.hpp"
#include "qf/oms/oms_types.hpp"
#include "qf/network/tcp_client.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace qf::protocol::fix {

// ─────────────────────────────────────────────────────────────
//  Execution report delivered by the gateway to the application
// ─────────────────────────────────────────────────────────────

struct ExecReport {
    std::string  cl_ord_id;            // client-assigned order ID
    std::string  order_id;             // exchange-assigned order ID
    std::string  exec_id;
    std::string  symbol;
    char         exec_type    = '\0';  // ExecType enum value
    char         ord_status   = '\0';  // OrdStatus enum value
    Side         side         = Side::Buy;
    double       avg_px       = 0.0;
    double       last_px      = 0.0;
    Quantity     last_qty     = 0;
    Quantity     cum_qty      = 0;
    Quantity     leaves_qty   = 0;
    std::string  text;                 // free-form text (reject reason, etc.)
};

// Callback types for the gateway.
using ExecReportCallback   = std::function<void(const ExecReport&)>;
using GatewayStateCallback = std::function<void(SessionState, SessionState)>;

// ─────────────────────────────────────────────────────────────
//  FixGateway — application-facing FIX 4.4 order gateway
// ─────────────────────────────────────────────────────────────
//
//  Combines FixSession (protocol) with TcpClient (transport) and
//  provides a typed API for order entry.
//
//  Usage:
//      asio::io_context io;
//      FixGateway gw(io, "MY_FIRM", "EXCHANGE");
//      gw.on_execution_report([](const ExecReport& rpt) { ... });
//      gw.connect("10.0.0.1", 9876);
//
//      auto cl_ord_id = gw.send_new_order(Symbol("AAPL"), Side::Buy,
//                                         100, double_to_price(185.05));
//      gw.send_cancel(cl_ord_id);
//

class FixGateway {
public:
    FixGateway(asio::io_context& io,
               const std::string& sender_comp_id,
               const std::string& target_comp_id,
               int heartbeat_interval_sec = 30)
        : io_(io)
        , tcp_(io)
        , session_(sender_comp_id, target_comp_id, heartbeat_interval_sec)
        , heartbeat_timer_(io)
    {
        // Wire session to TCP transport
        session_.set_send_callback([this](const std::string& raw) {
            if (tcp_.is_connected())
                tcp_.send(reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
        });

        // Forward application messages from the session to our dispatcher
        session_.set_app_message_callback([this](const FixMessage& msg) {
            on_app_message(msg);
        });

        // TCP callbacks
        tcp_.set_on_connect([this]() {
            on_tcp_connected();
        });

        tcp_.set_on_disconnect([this]() {
            session_.on_disconnect();
        });

        tcp_.set_on_data([this](const uint8_t* data, size_t len) {
            on_tcp_data(data, len);
        });

        tcp_.set_reconnect_interval_ms(5000);
    }

    // ── Connection management ──

    void connect(const std::string& host, uint16_t port) {
        tcp_.connect(host, port);
    }

    void disconnect() {
        session_.send_logout("Client disconnect");
        // TCP close will happen after logout ack or timeout
    }

    bool is_active() const {
        return session_.state() == SessionState::Active;
    }

    SessionState session_state() const {
        return session_.state();
    }

    // ── Callbacks ──

    void on_execution_report(ExecReportCallback cb) {
        std::lock_guard<std::mutex> lk(mu_);
        exec_cb_ = std::move(cb);
    }

    void on_state_change(GatewayStateCallback cb) {
        session_.set_state_callback(std::move(cb));
    }

    // ── Order entry ──

    // Send a new order.  Returns the ClOrdID assigned to this order.
    // The caller can use this ID to cancel or track the order.
    //
    // Parameters:
    //   symbol  — instrument symbol (max 8 chars)
    //   side    — Buy or Sell
    //   qty     — order quantity
    //   price   — limit price in fixed-point (raw / 10000.0); 0 for market
    //   tif     — TimeInForce (default: Day)
    //
    std::string send_new_order(const Symbol& symbol,
                               Side side,
                               Quantity qty,
                               Price price = 0,
                               char tif = TimeInForce::Day)
    {
        std::lock_guard<std::mutex> lk(mu_);

        std::string cl_ord_id = generate_cl_ord_id();
        char ord_type = (price == 0) ? OrdType::Market : OrdType::Limit;
        char fix_side = (side == Side::Buy) ? FixSide::Buy : FixSide::Sell;

        // Extract symbol as a null-terminated string
        char sym_buf[SYMBOL_LENGTH + 1] = {};
        std::memcpy(sym_buf, symbol.data, SYMBOL_LENGTH);

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::NewOrderSingle)
           .set_tag(Tag::ClOrdID, cl_ord_id)
           .set_tag(Tag::HandlInst, HandlInst::AutoPrivate)
           .set_tag(Tag::Symbol, sym_buf)
           .set_tag(Tag::Side, fix_side)
           .set_tag(Tag::TransactTime, FixSession::sending_time_str())
           .set_tag(Tag::OrderQty, qty)
           .set_tag(Tag::OrdType, ord_type)
           .set_tag(Tag::TimeInForce, tif);

        if (price != 0)
            msg.set_tag(Tag::Price, price_to_double(price));

        session_.send_app_message(msg);

        // Track the ClOrdID
        pending_orders_.emplace(cl_ord_id, PendingOrder{symbol, side, qty, price});

        return cl_ord_id;
    }

    // Cancel an outstanding order by its ClOrdID.
    // Returns the new OrigClOrdID used for the cancel request.
    std::string send_cancel(const std::string& cl_ord_id) {
        std::lock_guard<std::mutex> lk(mu_);

        std::string cancel_id = generate_cl_ord_id();

        // Look up original order info
        auto it = pending_orders_.find(cl_ord_id);

        char sym_buf[SYMBOL_LENGTH + 1] = {};
        char fix_side = FixSide::Buy;
        Quantity qty = 0;

        if (it != pending_orders_.end()) {
            std::memcpy(sym_buf, it->second.symbol.data, SYMBOL_LENGTH);
            fix_side = (it->second.side == Side::Buy) ? FixSide::Buy : FixSide::Sell;
            qty = it->second.quantity;
        }

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::OrderCancelRequest)
           .set_tag(Tag::ClOrdID, cancel_id)
           .set_tag(Tag::OrigClOrdID, cl_ord_id)
           .set_tag(Tag::Symbol, sym_buf)
           .set_tag(Tag::Side, fix_side)
           .set_tag(Tag::TransactTime, FixSession::sending_time_str())
           .set_tag(Tag::OrderQty, qty);

        session_.send_app_message(msg);

        // Map the cancel ID back to the original order
        cancel_to_orig_[cancel_id] = cl_ord_id;

        return cancel_id;
    }

    // ── ClOrdID / OrderId mapping ──

    // Register an external mapping from ClOrdID to internal OrderId.
    // This lets the OMS correlate FIX execution reports with internal orders.
    void map_order_id(const std::string& cl_ord_id, OrderId internal_id) {
        std::lock_guard<std::mutex> lk(mu_);
        clordid_to_internal_[cl_ord_id] = internal_id;
    }

    // Look up the internal OrderId for a ClOrdID.
    std::optional<OrderId> get_internal_id(const std::string& cl_ord_id) const {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = clordid_to_internal_.find(cl_ord_id);
        if (it == clordid_to_internal_.end()) return std::nullopt;
        return it->second;
    }

    // ── Heartbeat timer ──
    //
    // Call start_heartbeat_timer() after construction to begin
    // the periodic heartbeat check on the io_context.

    void start_heartbeat_timer() {
        schedule_heartbeat_check();
    }

    // ── Access to internals (for testing / diagnostics) ──

    FixSession& session() { return session_; }
    const FixSession& session() const { return session_; }

    network::TcpClient& tcp_client() { return tcp_; }

private:
    // ── Internal types ──

    struct PendingOrder {
        Symbol   symbol;
        Side     side;
        Quantity quantity;
        Price    price;
    };

    // ── State ──

    asio::io_context& io_;
    network::TcpClient tcp_;
    FixSession session_;
    asio::steady_timer heartbeat_timer_;

    mutable std::mutex mu_;

    ExecReportCallback exec_cb_;

    std::atomic<uint64_t> cl_ord_seq_{1};

    // Tracking maps
    std::unordered_map<std::string, PendingOrder> pending_orders_;
    std::unordered_map<std::string, std::string>  cancel_to_orig_;
    std::unordered_map<std::string, OrderId>      clordid_to_internal_;

    // Receive buffer for TCP framing
    std::string recv_buf_;

    // ── ClOrdID generation ──
    //
    // Format: "QF-<nanosecond_timestamp>-<sequence>"
    // Guarantees uniqueness within a single process.

    std::string generate_cl_ord_id() {
        uint64_t seq = cl_ord_seq_.fetch_add(1, std::memory_order_relaxed);
        return "QF-" + std::to_string(Clock::now_ns()) + "-" + std::to_string(seq);
    }

    // ── TCP event handlers ──

    void on_tcp_connected() {
        // Initiate FIX logon as soon as TCP connects
        session_.send_logon(/* reset_seq = */ false);
    }

    void on_tcp_data(const uint8_t* data, size_t len) {
        // Accumulate data and extract complete FIX messages
        recv_buf_.append(reinterpret_cast<const char*>(data), len);

        while (!recv_buf_.empty()) {
            size_t msg_len = FixMessage::extract_message_length(
                recv_buf_.data(), recv_buf_.size());
            if (msg_len == 0) break;   // incomplete message

            auto parsed = FixMessage::parse(recv_buf_.data(), msg_len);
            if (parsed)
                session_.on_message_received(*parsed);

            recv_buf_.erase(0, msg_len);
        }

        // Safety: if the buffer grows unreasonably, clear it (broken stream).
        if (recv_buf_.size() > 1024 * 1024) {
            recv_buf_.clear();
        }
    }

    // ── Application message dispatch ──

    void on_app_message(const FixMessage& msg) {
        char mt = msg.msg_type();

        switch (mt) {
            case MsgType::ExecutionReport:
                handle_execution_report(msg);
                break;
            case MsgType::OrderCancelReject:
                handle_cancel_reject(msg);
                break;
            default:
                // Unknown application message — ignore
                break;
        }
    }

    void handle_execution_report(const FixMessage& msg) {
        ExecReport rpt;

        auto val = msg.get_tag(Tag::ClOrdID);     if (val) rpt.cl_ord_id  = *val;
        val = msg.get_tag(Tag::OrderID);           if (val) rpt.order_id   = *val;
        val = msg.get_tag(Tag::ExecID);            if (val) rpt.exec_id    = *val;
        val = msg.get_tag(Tag::Symbol);            if (val) rpt.symbol     = *val;
        val = msg.get_tag(Tag::Text);              if (val) rpt.text       = *val;

        auto ch = msg.get_tag(Tag::ExecType);      if (ch && !ch->empty()) rpt.exec_type  = ch->front();
        ch = msg.get_tag(Tag::OrdStatus);          if (ch && !ch->empty()) rpt.ord_status = ch->front();

        auto side_val = msg.get_tag(Tag::Side);
        if (side_val && !side_val->empty())
            rpt.side = (side_val->front() == FixSide::Buy) ? Side::Buy : Side::Sell;

        auto dv = msg.get_tag_double(Tag::AvgPx);   if (dv) rpt.avg_px   = *dv;
        dv = msg.get_tag_double(Tag::LastPx);        if (dv) rpt.last_px  = *dv;

        auto iv = msg.get_tag_int(Tag::LastQty);     if (iv) rpt.last_qty   = static_cast<Quantity>(*iv);
        iv = msg.get_tag_int(Tag::CumQty);           if (iv) rpt.cum_qty    = static_cast<Quantity>(*iv);
        iv = msg.get_tag_int(Tag::LeavesQty);        if (iv) rpt.leaves_qty = static_cast<Quantity>(*iv);

        // Notify application
        std::lock_guard<std::mutex> lk(mu_);

        // Update pending order tracking
        if (rpt.ord_status == OrdStatus::Filled ||
            rpt.ord_status == OrdStatus::Cancelled ||
            rpt.ord_status == OrdStatus::Rejected) {
            pending_orders_.erase(rpt.cl_ord_id);
        }

        if (exec_cb_) exec_cb_(rpt);
    }

    void handle_cancel_reject(const FixMessage& msg) {
        // Translate cancel reject into an ExecReport-like notification
        ExecReport rpt;

        auto val = msg.get_tag(Tag::ClOrdID);     if (val) rpt.cl_ord_id  = *val;
        val = msg.get_tag(Tag::OrderID);           if (val) rpt.order_id   = *val;
        val = msg.get_tag(Tag::Text);              if (val) rpt.text       = *val;

        rpt.exec_type  = ExecType::Rejected;
        rpt.ord_status = OrdStatus::Rejected;

        // Resolve the original ClOrdID from the cancel mapping
        auto orig = msg.get_tag(Tag::OrigClOrdID);
        if (orig) {
            auto it = cancel_to_orig_.find(*orig);
            if (it != cancel_to_orig_.end()) {
                rpt.cl_ord_id = it->second;
                cancel_to_orig_.erase(it);
            }
        }

        std::lock_guard<std::mutex> lk(mu_);
        if (exec_cb_) exec_cb_(rpt);
    }

    // ── Heartbeat timer loop ──

    void schedule_heartbeat_check() {
        heartbeat_timer_.expires_after(std::chrono::seconds(1));
        heartbeat_timer_.async_wait([this](const asio::error_code& ec) {
            if (ec) return;   // timer cancelled
            session_.check_heartbeat();
            schedule_heartbeat_check();   // reschedule
        });
    }

};

}  // namespace qf::protocol::fix
