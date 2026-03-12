#pragma once

#include "qf/oms/oms_types.hpp"
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace qf::oms {

// Sequenced wrapper around an OrderEvent for the journal.
struct JournalEntry {
    uint64_t   sequence_number{0};
    OrderEvent event;
};

// Append-only audit log of all order lifecycle events.
// Thread-safe: single writer, multiple concurrent readers.
class OrderJournal {
public:
    // Append an event. Assigns a monotonically increasing sequence number.
    // Returns the assigned sequence number.
    uint64_t append(const OrderEvent& event);

    // Retrieve full event history for a specific order.
    std::vector<JournalEntry> get_history(OrderId order_id) const;

    // Return the last n events across all orders (most recent last).
    std::vector<JournalEntry> get_recent(size_t n) const;

    // Total number of journal entries.
    size_t size() const;

    // Flush the journal contents to a binary file for persistence.
    // Returns true on success.
    bool flush_to_file(const std::string& path) const;

    // Load journal contents from a binary file.
    // Returns true on success.
    bool load_from_file(const std::string& path);

private:
    mutable std::shared_mutex mutex_;
    std::vector<JournalEntry> entries_;
    // Secondary index: order_id -> indices into entries_
    std::unordered_map<OrderId, std::vector<size_t>> index_;
    uint64_t next_sequence_{1};
};

}  // namespace qf::oms
