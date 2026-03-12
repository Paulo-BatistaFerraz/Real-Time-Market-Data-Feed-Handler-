#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "qf/data/data_types.hpp"
#include "qf/data/recorder/binary_writer.hpp"
#include "qf/data/storage/time_index.hpp"

namespace qf::data {

// TickStore — read/write access to tick data files with time-based lookups.
// Uses a TimeIndex for efficient range queries on timestamp.
class TickStore {
public:
    // Open a tick store backed by the given file path.
    // If the file exists, the index is built automatically.
    // If the file does not exist, it is created when write() is first called.
    explicit TickStore(const std::string& path);

    ~TickStore();

    TickStore(const TickStore&) = delete;
    TickStore& operator=(const TickStore&) = delete;

    // Read all TickRecords in [start_time, end_time] for the given symbol.
    // If symbol is default-constructed (all zeros), returns all symbols.
    std::vector<TickRecord> read_range(const Symbol& symbol,
                                       Timestamp start_time,
                                       Timestamp end_time) const;

    // Read all TickRecords in [start_time, end_time] regardless of symbol.
    std::vector<TickRecord> read_range(Timestamp start_time,
                                       Timestamp end_time) const;

    // Append a tick record to the file. Returns the byte offset written.
    uint64_t write(const TickRecord& record);

    // Flush any buffered writes to disk.
    void flush();

    // Close the store (flushes writes, releases file handles).
    void close();

    // Rebuild the time index from the underlying file.
    uint64_t rebuild_index();

    // Number of records in the index (reflects file contents after last build).
    size_t record_count() const { return index_.size(); }

    // Access the underlying time index.
    const TimeIndex& index() const { return index_; }

    // Whether the store file exists and has been indexed.
    bool is_indexed() const { return !index_.empty(); }

    // The file path backing this store.
    const std::string& path() const { return path_; }

private:
    std::string  path_;
    TimeIndex    index_;
    BinaryWriter* writer_{nullptr};
};

}  // namespace qf::data
