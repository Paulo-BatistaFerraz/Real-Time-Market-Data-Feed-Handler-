#include "qf/oms/state_machine/order_journal.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace qf::oms {

uint64_t OrderJournal::append(const OrderEvent& event) {
    std::unique_lock lock(mutex_);
    uint64_t seq = next_sequence_++;

    JournalEntry entry;
    entry.sequence_number = seq;
    entry.event = event;

    size_t idx = entries_.size();
    entries_.push_back(std::move(entry));
    index_[event.order_id].push_back(idx);

    return seq;
}

std::vector<JournalEntry> OrderJournal::get_history(OrderId order_id) const {
    std::shared_lock lock(mutex_);
    std::vector<JournalEntry> result;

    auto it = index_.find(order_id);
    if (it == index_.end()) return result;

    result.reserve(it->second.size());
    for (size_t idx : it->second) {
        result.push_back(entries_[idx]);
    }
    return result;
}

std::vector<JournalEntry> OrderJournal::get_recent(size_t n) const {
    std::shared_lock lock(mutex_);
    if (n == 0 || entries_.empty()) return {};

    size_t count = std::min(n, entries_.size());
    size_t start = entries_.size() - count;
    return {entries_.begin() + static_cast<ptrdiff_t>(start), entries_.end()};
}

size_t OrderJournal::size() const {
    std::shared_lock lock(mutex_);
    return entries_.size();
}

// Binary file format (all little-endian, native struct layout):
//   Header:  "OJOURNAL" (8 bytes magic) | uint64_t entry_count
//   Per entry:
//     uint64_t sequence_number
//     uint64_t order_id
//     uint8_t  from_state
//     uint8_t  to_state
//     uint64_t timestamp
//     uint32_t reason_length
//     char[]   reason (reason_length bytes)

static constexpr char MAGIC[8] = {'O','J','O','U','R','N','A','L'};

bool OrderJournal::flush_to_file(const std::string& path) const {
    std::shared_lock lock(mutex_);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    // Header
    out.write(MAGIC, 8);
    uint64_t count = entries_.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Entries
    for (const auto& entry : entries_) {
        out.write(reinterpret_cast<const char*>(&entry.sequence_number), sizeof(entry.sequence_number));
        out.write(reinterpret_cast<const char*>(&entry.event.order_id), sizeof(entry.event.order_id));

        auto from = static_cast<uint8_t>(entry.event.from_state);
        auto to   = static_cast<uint8_t>(entry.event.to_state);
        out.write(reinterpret_cast<const char*>(&from), sizeof(from));
        out.write(reinterpret_cast<const char*>(&to), sizeof(to));

        out.write(reinterpret_cast<const char*>(&entry.event.timestamp), sizeof(entry.event.timestamp));

        uint32_t reason_len = static_cast<uint32_t>(entry.event.reason.size());
        out.write(reinterpret_cast<const char*>(&reason_len), sizeof(reason_len));
        if (reason_len > 0) {
            out.write(entry.event.reason.data(), reason_len);
        }
    }

    return out.good();
}

bool OrderJournal::load_from_file(const std::string& path) {
    std::unique_lock lock(mutex_);

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    // Verify magic
    char magic[8];
    in.read(magic, 8);
    if (std::memcmp(magic, MAGIC, 8) != 0) return false;

    uint64_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    entries_.clear();
    index_.clear();
    entries_.reserve(static_cast<size_t>(count));
    next_sequence_ = 1;

    for (uint64_t i = 0; i < count; ++i) {
        JournalEntry entry;
        in.read(reinterpret_cast<char*>(&entry.sequence_number), sizeof(entry.sequence_number));
        in.read(reinterpret_cast<char*>(&entry.event.order_id), sizeof(entry.event.order_id));

        uint8_t from = 0, to = 0;
        in.read(reinterpret_cast<char*>(&from), sizeof(from));
        in.read(reinterpret_cast<char*>(&to), sizeof(to));
        entry.event.from_state = static_cast<OrderState>(from);
        entry.event.to_state   = static_cast<OrderState>(to);

        in.read(reinterpret_cast<char*>(&entry.event.timestamp), sizeof(entry.event.timestamp));

        uint32_t reason_len = 0;
        in.read(reinterpret_cast<char*>(&reason_len), sizeof(reason_len));
        if (reason_len > 0) {
            entry.event.reason.resize(reason_len);
            in.read(&entry.event.reason[0], reason_len);
        }

        if (!in.good()) return false;

        size_t idx = entries_.size();
        if (entry.sequence_number >= next_sequence_) {
            next_sequence_ = entry.sequence_number + 1;
        }
        entries_.push_back(std::move(entry));
        index_[entries_[idx].event.order_id].push_back(idx);
    }

    return true;
}

}  // namespace qf::oms
