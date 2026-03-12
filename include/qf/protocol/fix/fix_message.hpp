#pragma once

#include "qf/protocol/fix/fix_fields.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace qf::protocol::fix {

// ─────────────────────────────────────────────────────────────
//  FixMessage — FIX 4.4 message builder / parser
// ─────────────────────────────────────────────────────────────
//
//  Builder usage:
//      FixMessage msg;
//      msg.set_tag(Tag::MsgType, "D")
//         .set_tag(Tag::SenderCompID, "CLIENT")
//         .set_tag(Tag::TargetCompID, "EXCHANGE")
//         .set_tag(Tag::MsgSeqNum, 42)
//         .set_tag(Tag::Symbol, "AAPL")
//         .set_tag(Tag::Side, "1")
//         .set_tag(Tag::OrderQty, 100)
//         .set_tag(Tag::OrdType, "2")
//         .set_tag(Tag::Price, "185.0500");
//      std::string wire = msg.serialize();
//
//  Parser usage:
//      auto parsed = FixMessage::parse(wire.data(), wire.size());
//      if (parsed) {
//          auto sym = parsed->get_tag(Tag::Symbol);
//      }
//
class FixMessage {
public:
    // ── Construction ──

    FixMessage() = default;

    // ── Builder interface (fluent, chainable) ──

    FixMessage& set_tag(int tag, const std::string& value) {
        // Overwrite if the tag already exists
        for (auto& [t, v] : fields_) {
            if (t == tag) { v = value; return *this; }
        }
        fields_.emplace_back(tag, value);
        return *this;
    }

    FixMessage& set_tag(int tag, std::string_view value) {
        return set_tag(tag, std::string(value));
    }

    FixMessage& set_tag(int tag, const char* value) {
        return set_tag(tag, std::string(value));
    }

    FixMessage& set_tag(int tag, char value) {
        return set_tag(tag, std::string(1, value));
    }

    FixMessage& set_tag(int tag, int64_t value) {
        return set_tag(tag, std::to_string(value));
    }

    FixMessage& set_tag(int tag, uint64_t value) {
        return set_tag(tag, std::to_string(value));
    }

    FixMessage& set_tag(int tag, int value) {
        return set_tag(tag, std::to_string(value));
    }

    FixMessage& set_tag(int tag, uint32_t value) {
        return set_tag(tag, std::to_string(value));
    }

    FixMessage& set_tag(int tag, double value) {
        // FIX prices: avoid trailing zeros from to_string, use snprintf
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.4f", value);
        return set_tag(tag, std::string(buf));
    }

    // ── Accessors ──

    std::optional<std::string> get_tag(int tag) const {
        for (const auto& [t, v] : fields_) {
            if (t == tag) return v;
        }
        return std::nullopt;
    }

    bool has_tag(int tag) const {
        return std::any_of(fields_.begin(), fields_.end(),
            [tag](const auto& f) { return f.first == tag; });
    }

    // Get tag as integer, returns nullopt on missing or non-numeric.
    std::optional<int64_t> get_tag_int(int tag) const {
        auto val = get_tag(tag);
        if (!val) return std::nullopt;
        try { return std::stoll(*val); }
        catch (...) { return std::nullopt; }
    }

    // Get tag as double.
    std::optional<double> get_tag_double(int tag) const {
        auto val = get_tag(tag);
        if (!val) return std::nullopt;
        try { return std::stod(*val); }
        catch (...) { return std::nullopt; }
    }

    // Get MsgType as a char (convenience).
    char msg_type() const {
        auto mt = get_tag(Tag::MsgType);
        return (mt && !mt->empty()) ? mt->front() : '\0';
    }

    // Get sequence number.
    uint32_t msg_seq_num() const {
        auto s = get_tag_int(Tag::MsgSeqNum);
        return s ? static_cast<uint32_t>(*s) : 0;
    }

    // Remove a tag (returns *this for chaining).
    FixMessage& remove_tag(int tag) {
        fields_.erase(
            std::remove_if(fields_.begin(), fields_.end(),
                [tag](const auto& f) { return f.first == tag; }),
            fields_.end());
        return *this;
    }

    // Number of fields (excluding auto-generated header/trailer).
    size_t field_count() const { return fields_.size(); }

    // Direct access to the underlying field list.
    const std::vector<std::pair<int, std::string>>& fields() const { return fields_; }

    // ── Serialization ──
    //
    // Produces a complete FIX message with:
    //   8=FIX.4.4 | 9=<body_length> | <body fields...> | 10=<checksum>
    //
    // The body is everything between tags 9 and 10 (inclusive of tag 35).
    // BeginString (8), BodyLength (9), and CheckSum (10) are auto-managed;
    // if the caller has set them in the field list they will be ignored
    // in the body and regenerated.

    std::string serialize() const {
        // 1. Build body: everything that is NOT tag 8, 9, or 10
        std::string body;
        for (const auto& [tag, val] : fields_) {
            if (tag == Tag::BeginString || tag == Tag::BodyLength || tag == Tag::CheckSum)
                continue;
            append_field(body, tag, val);
        }

        // 2. Build header: 8=FIX.4.4 SOH  9=<len> SOH
        std::string header;
        append_field(header, Tag::BeginString, FIX_BEGIN_STRING);
        append_field(header, Tag::BodyLength, std::to_string(body.size()));

        // 3. Compute checksum over header + body (from tag 8= onward)
        //    Per spec, checksum is sum of all bytes BEFORE the checksum field, mod 256.
        //    The checksum covers everything including 8= and 9= fields.
        //    CORRECTION: FIX spec says checksum covers everything from tag 8 onward,
        //    but BodyLength counts bytes from AFTER the 9= tag SOH to before 10=.
        //    Checksum is the sum of ALL bytes from "8=..." up to and including
        //    the SOH after the last body field.
        std::string pre_checksum = header + body;
        uint32_t cksum = 0;
        for (unsigned char c : pre_checksum)
            cksum += c;
        cksum %= 256;

        char cksum_str[4];
        std::snprintf(cksum_str, sizeof(cksum_str), "%03u", cksum);

        // 4. Assemble: header + body + 10=xxx SOH
        std::string result;
        result.reserve(pre_checksum.size() + 8);
        result += pre_checksum;
        append_field(result, Tag::CheckSum, cksum_str);

        return result;
    }

    // ── Parsing ──
    //
    // Parses a raw FIX message from a byte buffer.
    // Returns nullopt on malformed input.

    static std::optional<FixMessage> parse(const char* data, size_t len) {
        if (!data || len == 0) return std::nullopt;

        FixMessage msg;
        std::string_view input(data, len);
        size_t pos = 0;

        while (pos < input.size()) {
            // Find '=' delimiter
            auto eq_pos = input.find('=', pos);
            if (eq_pos == std::string_view::npos) break;

            // Find SOH delimiter
            auto soh_pos = input.find(SOH, eq_pos + 1);
            if (soh_pos == std::string_view::npos) {
                // Last field may lack trailing SOH in some implementations
                soh_pos = input.size();
            }

            // Extract tag number and value
            std::string tag_str(input.substr(pos, eq_pos - pos));
            std::string value(input.substr(eq_pos + 1, soh_pos - eq_pos - 1));

            int tag = 0;
            try { tag = std::stoi(tag_str); }
            catch (...) { return std::nullopt; }   // malformed tag

            msg.fields_.emplace_back(tag, std::move(value));
            pos = soh_pos + 1;
        }

        if (msg.fields_.empty()) return std::nullopt;

        return msg;
    }

    // Convenience: parse from std::string
    static std::optional<FixMessage> parse(const std::string& raw) {
        return parse(raw.data(), raw.size());
    }

    // ── Validation ──

    // Verify checksum on a received message.
    bool validate_checksum() const {
        auto declared = get_tag(Tag::CheckSum);
        if (!declared) return false;

        // Recompute: checksum is over everything before the 10= field.
        // We need to reserialize without the checksum tag.
        std::string body;
        for (const auto& [tag, val] : fields_) {
            if (tag == Tag::CheckSum) continue;
            append_field(body, tag, val);
        }

        uint32_t cksum = 0;
        for (unsigned char c : body)
            cksum += c;
        cksum %= 256;

        char expected[4];
        std::snprintf(expected, sizeof(expected), "%03u", cksum);

        return *declared == expected;
    }

    // Verify that BodyLength matches the actual body size.
    bool validate_body_length() const {
        auto declared = get_tag_int(Tag::BodyLength);
        if (!declared) return false;

        // Body = everything after 9=<len>SOH up to (not including) 10=
        std::string body;
        for (const auto& [tag, val] : fields_) {
            if (tag == Tag::BeginString || tag == Tag::BodyLength || tag == Tag::CheckSum)
                continue;
            append_field(body, tag, val);
        }

        return static_cast<int64_t>(body.size()) == *declared;
    }

    // Quick structural check: has required header fields.
    bool is_valid() const {
        return has_tag(Tag::BeginString) &&
               has_tag(Tag::BodyLength) &&
               has_tag(Tag::MsgType) &&
               has_tag(Tag::SenderCompID) &&
               has_tag(Tag::TargetCompID) &&
               has_tag(Tag::MsgSeqNum) &&
               has_tag(Tag::SendingTime) &&
               has_tag(Tag::CheckSum);
    }

    // ── Framing helpers ──

    // Given a buffer that may contain multiple concatenated FIX messages,
    // extract the length of the first complete message (including checksum).
    // Returns 0 if no complete message is available.
    static size_t extract_message_length(const char* data, size_t len) {
        std::string_view input(data, len);

        // Find "8=FIX" at the start
        if (input.substr(0, 2) != "8=") return 0;

        // Find 9=<BodyLength>
        auto tag9_pos = input.find(std::string(1, SOH) + "9=");
        if (tag9_pos == std::string_view::npos) return 0;

        auto eq_pos = tag9_pos + 2;    // position of '=' in "9="
        auto soh_after_9 = input.find(SOH, eq_pos + 1);
        if (soh_after_9 == std::string_view::npos) return 0;

        std::string body_len_str(input.substr(eq_pos + 1, soh_after_9 - eq_pos - 1));
        int body_length = 0;
        try { body_length = std::stoi(body_len_str); }
        catch (...) { return 0; }

        // Message ends at: body_start + body_length + "10=xxx" + SOH
        size_t body_start = soh_after_9 + 1;
        size_t checksum_end = body_start + body_length + 7;  // "10=xxx\x01" = 7 bytes

        if (checksum_end > len) return 0;   // incomplete
        return checksum_end;
    }

private:
    std::vector<std::pair<int, std::string>> fields_;

    static void append_field(std::string& out, int tag, const std::string& value) {
        out += std::to_string(tag);
        out += '=';
        out += value;
        out += SOH;
    }
};

}  // namespace qf::protocol::fix
