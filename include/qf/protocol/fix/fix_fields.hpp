#pragma once

// FIX 4.4 tag constants and enumerated values.
// Reference: FIX Protocol 4.4 specification, Volume 1-5.

namespace qf::protocol::fix {

// ─────────────────────────────────────────────────────────────
//  Tag numbers (field identifiers)
// ─────────────────────────────────────────────────────────────

namespace Tag {
    // ── Header / trailer ──
    constexpr int BeginString       = 8;
    constexpr int BodyLength        = 9;
    constexpr int CheckSum          = 10;
    constexpr int MsgSeqNum         = 34;
    constexpr int MsgType           = 35;
    constexpr int SenderCompID      = 49;
    constexpr int SendingTime       = 52;
    constexpr int TargetCompID      = 56;

    // ── Session-level ──
    constexpr int EncryptMethod     = 98;
    constexpr int HeartBtInt        = 108;
    constexpr int TestReqID         = 112;
    constexpr int ResetSeqNumFlag   = 141;
    constexpr int BeginSeqNo        = 7;
    constexpr int EndSeqNo          = 16;
    constexpr int NewSeqNo          = 36;
    constexpr int GapFillFlag       = 123;
    constexpr int RefSeqNum         = 45;
    constexpr int Text              = 58;
    constexpr int SessionRejectReason = 373;

    // ── Order entry ──
    constexpr int Account           = 1;
    constexpr int AvgPx             = 6;
    constexpr int ClOrdID           = 11;
    constexpr int CumQty            = 14;
    constexpr int ExecID            = 17;
    constexpr int ExecTransType     = 20;   // FIX 4.2 compat
    constexpr int HandlInst         = 21;
    constexpr int OrderID           = 37;
    constexpr int OrderQty          = 38;
    constexpr int OrdStatus         = 39;
    constexpr int OrdType           = 40;
    constexpr int OrigClOrdID       = 41;
    constexpr int Price             = 44;
    constexpr int Side              = 54;
    constexpr int Symbol            = 55;
    constexpr int TimeInForce       = 59;
    constexpr int TransactTime      = 60;
    constexpr int ExecType          = 150;
    constexpr int LeavesQty         = 151;
    constexpr int LastQty           = 32;
    constexpr int LastPx            = 31;
    constexpr int CxlRejReason      = 102;
    constexpr int CxlRejResponseTo  = 434;
}  // namespace Tag

// ─────────────────────────────────────────────────────────────
//  MsgType values (tag 35)
// ─────────────────────────────────────────────────────────────

namespace MsgType {
    constexpr char Heartbeat         = '0';
    constexpr char TestRequest       = '1';
    constexpr char ResendRequest     = '2';
    constexpr char Reject            = '3';
    constexpr char SequenceReset     = '4';
    constexpr char Logout            = '5';
    constexpr char Logon             = 'A';
    constexpr char NewOrderSingle    = 'D';
    constexpr char OrderCancelRequest = 'F';
    constexpr char ExecutionReport   = '8';
    constexpr char OrderCancelReject = '9';
}  // namespace MsgType

// ─────────────────────────────────────────────────────────────
//  Enumerated field values
// ─────────────────────────────────────────────────────────────

namespace OrdType {
    constexpr char Market = '1';
    constexpr char Limit  = '2';
}

namespace FixSide {
    constexpr char Buy  = '1';
    constexpr char Sell = '2';
}

namespace TimeInForce {
    constexpr char Day = '0';
    constexpr char GTC = '1';   // Good Till Cancel
    constexpr char IOC = '3';   // Immediate or Cancel
    constexpr char FOK = '4';   // Fill or Kill
}

namespace OrdStatus {
    constexpr char New          = '0';
    constexpr char PartialFill  = '1';
    constexpr char Filled       = '2';
    constexpr char Cancelled    = '4';
    constexpr char Rejected     = '8';
    constexpr char PendingNew   = 'A';
    constexpr char PendingCancel = '6';
}

namespace ExecType {
    constexpr char New          = '0';
    constexpr char PartialFill  = '1';
    constexpr char Fill         = '2';
    constexpr char Cancelled    = '4';
    constexpr char Rejected     = '8';
    constexpr char PendingNew   = 'A';
    constexpr char PendingCancel = '6';
}

namespace HandlInst {
    constexpr char AutoPrivate  = '1';   // Automated, private
    constexpr char AutoPublic   = '2';   // Automated, public
    constexpr char Manual       = '3';
}

namespace EncryptMethod {
    constexpr int None = 0;
}

constexpr const char* FIX_BEGIN_STRING = "FIX.4.4";
constexpr char SOH = '\x01';   // FIX field delimiter

}  // namespace qf::protocol::fix
