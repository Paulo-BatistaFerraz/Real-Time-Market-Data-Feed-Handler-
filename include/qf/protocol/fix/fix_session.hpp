#pragma once

#include "qf/protocol/fix/fix_message.hpp"
#include "qf/protocol/fix/fix_fields.hpp"
#include "qf/common/clock.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace qf::protocol::fix {

// ─────────────────────────────────────────────────────────────
//  FixSession — FIX 4.4 session-layer state machine
// ─────────────────────────────────────────────────────────────
//
//  Manages the session protocol: logon/logout handshake, sequence
//  numbering, heartbeat/TestRequest exchange, and gap detection.
//
//  This class does NOT own a socket. Outbound data flows through
//  the SendCallback; inbound messages arrive via on_message_received().
//
//  Thread safety: all public methods are mutex-protected.  The
//  heartbeat timer check (check_heartbeat) should be called
//  periodically by the owning event loop.
//

enum class SessionState : uint8_t {
    Disconnected = 0,
    LogonSent    = 1,
    Active       = 2,
    LogoutSent   = 3
};

inline const char* to_string(SessionState s) {
    switch (s) {
        case SessionState::Disconnected: return "Disconnected";
        case SessionState::LogonSent:    return "LogonSent";
        case SessionState::Active:       return "Active";
        case SessionState::LogoutSent:   return "LogoutSent";
    }
    return "Unknown";
}

// Callback types used by FixSession.
using SendCallback            = std::function<void(const std::string& raw)>;
using AppMessageCallback      = std::function<void(const FixMessage& msg)>;
using SessionStateCallback    = std::function<void(SessionState old_state, SessionState new_state)>;

class FixSession {
public:
    // ── Construction ──

    FixSession() = default;

    explicit FixSession(const std::string& sender_comp_id,
                        const std::string& target_comp_id,
                        int heartbeat_interval_sec = 30)
        : sender_comp_id_(sender_comp_id)
        , target_comp_id_(target_comp_id)
        , heartbeat_interval_sec_(heartbeat_interval_sec)
    {}

    // ── Configuration ──

    void set_comp_ids(const std::string& sender, const std::string& target) {
        std::lock_guard<std::mutex> lk(mu_);
        sender_comp_id_ = sender;
        target_comp_id_ = target;
    }

    void set_heartbeat_interval(int seconds) {
        std::lock_guard<std::mutex> lk(mu_);
        heartbeat_interval_sec_ = seconds;
    }

    void set_send_callback(SendCallback cb) {
        std::lock_guard<std::mutex> lk(mu_);
        send_cb_ = std::move(cb);
    }

    void set_app_message_callback(AppMessageCallback cb) {
        std::lock_guard<std::mutex> lk(mu_);
        app_msg_cb_ = std::move(cb);
    }

    void set_state_callback(SessionStateCallback cb) {
        std::lock_guard<std::mutex> lk(mu_);
        state_cb_ = std::move(cb);
    }

    // ── Accessors ──

    SessionState state() const {
        std::lock_guard<std::mutex> lk(mu_);
        return state_;
    }

    uint32_t next_outbound_seq() const {
        std::lock_guard<std::mutex> lk(mu_);
        return outbound_seq_;
    }

    uint32_t expected_inbound_seq() const {
        std::lock_guard<std::mutex> lk(mu_);
        return inbound_seq_;
    }

    const std::string& sender_comp_id() const { return sender_comp_id_; }
    const std::string& target_comp_id() const { return target_comp_id_; }

    // ── Outbound session messages ──

    void send_logon(bool reset_seq = false) {
        std::lock_guard<std::mutex> lk(mu_);

        if (reset_seq) {
            outbound_seq_ = 1;
            inbound_seq_  = 1;
        }

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::Logon)
           .set_tag(Tag::EncryptMethod, EncryptMethod::None)
           .set_tag(Tag::HeartBtInt, heartbeat_interval_sec_);

        if (reset_seq)
            msg.set_tag(Tag::ResetSeqNumFlag, "Y");

        send_message_locked(msg);
        transition_locked(SessionState::LogonSent);
    }

    void send_logout(const std::string& text = "") {
        std::lock_guard<std::mutex> lk(mu_);

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::Logout);
        if (!text.empty())
            msg.set_tag(Tag::Text, text);

        send_message_locked(msg);
        transition_locked(SessionState::LogoutSent);
    }

    void send_heartbeat(const std::string& test_req_id = "") {
        std::lock_guard<std::mutex> lk(mu_);

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::Heartbeat);
        if (!test_req_id.empty())
            msg.set_tag(Tag::TestReqID, test_req_id);

        send_message_locked(msg);
    }

    void send_test_request() {
        std::lock_guard<std::mutex> lk(mu_);

        std::string test_id = "TR-" + std::to_string(Clock::now_ns());
        pending_test_req_id_ = test_id;

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::TestRequest)
           .set_tag(Tag::TestReqID, test_id);

        send_message_locked(msg);
        test_request_sent_time_ = steady_now();
    }

    void send_resend_request(uint32_t begin_seq, uint32_t end_seq) {
        std::lock_guard<std::mutex> lk(mu_);

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::ResendRequest)
           .set_tag(Tag::BeginSeqNo, static_cast<int>(begin_seq))
           .set_tag(Tag::EndSeqNo, static_cast<int>(end_seq));

        send_message_locked(msg);
    }

    void send_sequence_reset(uint32_t new_seq, bool gap_fill = false) {
        std::lock_guard<std::mutex> lk(mu_);

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::SequenceReset)
           .set_tag(Tag::NewSeqNo, static_cast<int>(new_seq));
        if (gap_fill)
            msg.set_tag(Tag::GapFillFlag, "Y");

        send_message_locked(msg);
    }

    void send_reject(uint32_t ref_seq, const std::string& text = "", int reason = -1) {
        std::lock_guard<std::mutex> lk(mu_);

        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::Reject)
           .set_tag(Tag::RefSeqNum, static_cast<int>(ref_seq));
        if (!text.empty())
            msg.set_tag(Tag::Text, text);
        if (reason >= 0)
            msg.set_tag(Tag::SessionRejectReason, reason);

        send_message_locked(msg);
    }

    // ── Send an application-level message (tag 35 already set by caller) ──
    //
    // This stamps header fields (SenderCompID, TargetCompID, MsgSeqNum,
    // SendingTime) and serializes.

    void send_app_message(FixMessage& msg) {
        std::lock_guard<std::mutex> lk(mu_);
        send_message_locked(msg);
    }

    // ── Inbound message processing ──
    //
    // Call this for every complete FIX message received from the
    // counterparty.  Session-level messages are handled internally;
    // application-level messages are forwarded to the app callback.

    void on_message_received(const FixMessage& msg) {
        std::lock_guard<std::mutex> lk(mu_);

        last_recv_time_ = steady_now();

        // Validate sender/target (swap perspective: their sender = our target)
        auto sender = msg.get_tag(Tag::SenderCompID);
        auto target = msg.get_tag(Tag::TargetCompID);
        if (sender && *sender != target_comp_id_) return;   // wrong counterparty
        if (target && *target != sender_comp_id_) return;

        // Sequence number check
        uint32_t seq = msg.msg_seq_num();
        char mt = msg.msg_type();

        // SequenceReset-GapFill can arrive with out-of-order seq
        if (mt == MsgType::SequenceReset) {
            handle_sequence_reset_locked(msg);
            return;
        }

        if (seq > inbound_seq_) {
            // Gap detected — request resend
            send_resend_request_locked(inbound_seq_, seq - 1);
            // Still process this message (queue in production; for now, accept)
        }

        if (seq < inbound_seq_ && mt != MsgType::SequenceReset) {
            // Duplicate or PossDup — ignore
            return;
        }

        // Accept this sequence
        if (seq >= inbound_seq_)
            inbound_seq_ = seq + 1;

        // Dispatch by message type
        switch (mt) {
            case MsgType::Logon:          handle_logon_locked(msg);          break;
            case MsgType::Logout:         handle_logout_locked(msg);         break;
            case MsgType::Heartbeat:      handle_heartbeat_locked(msg);      break;
            case MsgType::TestRequest:    handle_test_request_locked(msg);   break;
            case MsgType::ResendRequest:  handle_resend_request_locked(msg); break;
            case MsgType::Reject:         handle_reject_locked(msg);         break;
            default:
                // Application message — forward to callback
                if (app_msg_cb_) app_msg_cb_(msg);
                break;
        }
    }

    // ── Heartbeat timer check ──
    //
    // Should be called periodically (e.g., every second) by the event loop.
    // Sends heartbeats and test requests as needed.

    void check_heartbeat() {
        std::lock_guard<std::mutex> lk(mu_);
        if (state_ != SessionState::Active) return;

        auto now = steady_now();
        auto interval = std::chrono::seconds(heartbeat_interval_sec_);

        // Send heartbeat if we haven't sent anything recently
        if (now - last_send_time_ >= interval) {
            FixMessage hb;
            hb.set_tag(Tag::MsgType, MsgType::Heartbeat);
            send_message_locked(hb);
        }

        // If we haven't received anything in heartbeat_interval + margin,
        // send a TestRequest
        auto recv_silence = now - last_recv_time_;
        if (recv_silence >= interval + std::chrono::seconds(2)) {
            if (!test_request_pending()) {
                std::string test_id = "TR-" + std::to_string(Clock::now_ns());
                pending_test_req_id_ = test_id;

                FixMessage tr;
                tr.set_tag(Tag::MsgType, MsgType::TestRequest)
                  .set_tag(Tag::TestReqID, test_id);
                send_message_locked(tr);
                test_request_sent_time_ = now;
            } else if (now - test_request_sent_time_ >= interval) {
                // TestRequest timed out — counterparty is dead
                FixMessage lo;
                lo.set_tag(Tag::MsgType, MsgType::Logout)
                  .set_tag(Tag::Text, "TestRequest timeout");
                send_message_locked(lo);
                transition_locked(SessionState::LogoutSent);
            }
        }
    }

    // ── Utility ──

    // Generate a SendingTime string: YYYYMMDD-HH:MM:SS.sss (UTC)
    static std::string sending_time_str() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm utc{};
#if defined(_MSC_VER) || defined(__MINGW32__)
        gmtime_s(&utc, &time_t_now);
#else
        gmtime_r(&time_t_now, &utc);
#endif

        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d:%02d:%02d.%03d",
            utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
            utc.tm_hour, utc.tm_min, utc.tm_sec,
            static_cast<int>(ms.count()));
        return buf;
    }

    // ── Reset ──

    void reset() {
        std::lock_guard<std::mutex> lk(mu_);
        state_         = SessionState::Disconnected;
        outbound_seq_  = 1;
        inbound_seq_   = 1;
        pending_test_req_id_.clear();
    }

    // Notify the session that the TCP connection has dropped.
    void on_disconnect() {
        std::lock_guard<std::mutex> lk(mu_);
        transition_locked(SessionState::Disconnected);
    }

private:
    // ── State ──
    mutable std::mutex mu_;

    std::string sender_comp_id_;
    std::string target_comp_id_;
    int heartbeat_interval_sec_ = 30;

    SessionState state_   = SessionState::Disconnected;
    uint32_t outbound_seq_ = 1;
    uint32_t inbound_seq_  = 1;

    // Heartbeat tracking
    using SteadyTimePoint = std::chrono::steady_clock::time_point;

    SteadyTimePoint last_send_time_          = std::chrono::steady_clock::now();
    SteadyTimePoint last_recv_time_          = std::chrono::steady_clock::now();
    SteadyTimePoint test_request_sent_time_  = {};
    std::string     pending_test_req_id_;

    // Callbacks
    SendCallback         send_cb_;
    AppMessageCallback   app_msg_cb_;
    SessionStateCallback state_cb_;

    // ── Helpers ──

    static SteadyTimePoint steady_now() {
        return std::chrono::steady_clock::now();
    }

    bool test_request_pending() const {
        return !pending_test_req_id_.empty();
    }

    void transition_locked(SessionState new_state) {
        if (state_ == new_state) return;
        auto old = state_;
        state_ = new_state;
        if (state_cb_) state_cb_(old, new_state);
    }

    // Stamp header fields and serialize + send.
    void send_message_locked(FixMessage& msg) {
        msg.set_tag(Tag::SenderCompID, sender_comp_id_)
           .set_tag(Tag::TargetCompID, target_comp_id_)
           .set_tag(Tag::MsgSeqNum, static_cast<int>(outbound_seq_++))
           .set_tag(Tag::SendingTime, sending_time_str());

        std::string wire = msg.serialize();
        last_send_time_ = steady_now();

        if (send_cb_) send_cb_(wire);
    }

    // ── Session message handlers ──

    void handle_logon_locked(const FixMessage& msg) {
        // Accept heartbeat interval from counterparty
        auto hb = msg.get_tag_int(Tag::HeartBtInt);
        if (hb && *hb > 0) heartbeat_interval_sec_ = static_cast<int>(*hb);

        if (state_ == SessionState::LogonSent) {
            // Counterparty confirmed our logon
            transition_locked(SessionState::Active);
        } else if (state_ == SessionState::Disconnected) {
            // Counterparty initiated logon — respond
            FixMessage reply;
            reply.set_tag(Tag::MsgType, MsgType::Logon)
                 .set_tag(Tag::EncryptMethod, EncryptMethod::None)
                 .set_tag(Tag::HeartBtInt, heartbeat_interval_sec_);
            send_message_locked(reply);
            transition_locked(SessionState::Active);
        }

        // Check ResetSeqNumFlag
        auto reset = msg.get_tag(Tag::ResetSeqNumFlag);
        if (reset && *reset == "Y") {
            inbound_seq_ = msg.msg_seq_num() + 1;
        }
    }

    void handle_logout_locked(const FixMessage& msg) {
        if (state_ == SessionState::LogoutSent) {
            // Clean mutual logout
            transition_locked(SessionState::Disconnected);
        } else {
            // Counterparty initiated logout — acknowledge
            FixMessage reply;
            reply.set_tag(Tag::MsgType, MsgType::Logout);
            auto text = msg.get_tag(Tag::Text);
            if (text) reply.set_tag(Tag::Text, *text);
            send_message_locked(reply);
            transition_locked(SessionState::Disconnected);
        }
    }

    void handle_heartbeat_locked(const FixMessage& msg) {
        // If this heartbeat is a response to our TestRequest, clear the pending flag.
        auto test_id = msg.get_tag(Tag::TestReqID);
        if (test_id && *test_id == pending_test_req_id_) {
            pending_test_req_id_.clear();
        }
    }

    void handle_test_request_locked(const FixMessage& msg) {
        // Respond with heartbeat containing the TestReqID
        auto test_id = msg.get_tag(Tag::TestReqID);
        FixMessage hb;
        hb.set_tag(Tag::MsgType, MsgType::Heartbeat);
        if (test_id) hb.set_tag(Tag::TestReqID, *test_id);
        send_message_locked(hb);
    }

    void handle_resend_request_locked(const FixMessage& msg) {
        // In a production system, we'd replay stored messages.
        // Here we respond with a SequenceReset-GapFill to skip the gap.
        auto begin = msg.get_tag_int(Tag::BeginSeqNo);
        auto end   = msg.get_tag_int(Tag::EndSeqNo);
        if (!begin) return;

        uint32_t end_seq = (end && *end > 0)
            ? static_cast<uint32_t>(*end + 1)
            : outbound_seq_;

        FixMessage sr;
        sr.set_tag(Tag::MsgType, MsgType::SequenceReset)
          .set_tag(Tag::GapFillFlag, "Y")
          .set_tag(Tag::NewSeqNo, static_cast<int>(end_seq));

        // GapFill messages use the BeginSeqNo as their MsgSeqNum
        sr.set_tag(Tag::MsgSeqNum, static_cast<int>(*begin));
        sr.set_tag(Tag::SenderCompID, sender_comp_id_);
        sr.set_tag(Tag::TargetCompID, target_comp_id_);
        sr.set_tag(Tag::SendingTime, sending_time_str());

        std::string wire = sr.serialize();
        last_send_time_ = steady_now();
        if (send_cb_) send_cb_(wire);
        // Note: do NOT increment outbound_seq_ for GapFill
    }

    void handle_sequence_reset_locked(const FixMessage& msg) {
        auto new_seq = msg.get_tag_int(Tag::NewSeqNo);
        if (!new_seq) return;

        auto gap_fill = msg.get_tag(Tag::GapFillFlag);
        if (gap_fill && *gap_fill == "Y") {
            // GapFill: advance expected seq if new_seq is higher
            if (static_cast<uint32_t>(*new_seq) > inbound_seq_)
                inbound_seq_ = static_cast<uint32_t>(*new_seq);
        } else {
            // Hard reset: unconditionally set expected seq
            inbound_seq_ = static_cast<uint32_t>(*new_seq);
        }
    }

    void handle_reject_locked(const FixMessage& /*msg*/) {
        // Session-level reject. In production, log the reject details.
        // The app layer may want a callback here; for now, no-op.
    }

    void send_resend_request_locked(uint32_t begin_seq, uint32_t end_seq) {
        FixMessage msg;
        msg.set_tag(Tag::MsgType, MsgType::ResendRequest)
           .set_tag(Tag::BeginSeqNo, static_cast<int>(begin_seq))
           .set_tag(Tag::EndSeqNo, static_cast<int>(end_seq));
        send_message_locked(msg);
    }
};

}  // namespace qf::protocol::fix
