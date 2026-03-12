#include "qf/data/storage/tick_store.hpp"
#include <cstdio>
#include <algorithm>

namespace qf::data {

TickStore::TickStore(const std::string& path)
    : path_(path) {
    // If the file already exists, build the index.
    FILE* test = std::fopen(path_.c_str(), "rb");
    if (test) {
        std::fclose(test);
        index_.build(path_);
    }
}

TickStore::~TickStore() {
    close();
}

std::vector<TickRecord> TickStore::read_range(const Symbol& symbol,
                                              Timestamp start_time,
                                              Timestamp end_time) const {
    std::vector<TickRecord> result;

    if (index_.empty()) {
        return result;
    }

    FILE* file = std::fopen(path_.c_str(), "rb");
    if (!file) {
        return result;
    }

    // Use the index to find the starting offset.
    uint64_t start_offset = index_.find(start_time);
    bool filter_symbol = (symbol.as_key() != 0);

    // If find() returned 0, there may be no records >= start_time.
    // But offset 0 is the header, not a record. Check if the first entry qualifies.
    if (start_offset == 0) {
        // No records at or after start_time.
        std::fclose(file);
        return result;
    }

    // Seek to the starting record.
    if (std::fseek(file, static_cast<long>(start_offset), SEEK_SET) != 0) {
        std::fclose(file);
        return result;
    }

    // Read records sequentially until we pass end_time.
    TickRecord record;
    while (std::fread(&record, sizeof(TickRecord), 1, file) == 1) {
        if (record.timestamp > end_time) {
            break;
        }

        if (record.timestamp >= start_time) {
            if (!filter_symbol || record.symbol == symbol) {
                result.push_back(record);
            }
        }
    }

    std::fclose(file);
    return result;
}

std::vector<TickRecord> TickStore::read_range(Timestamp start_time,
                                              Timestamp end_time) const {
    Symbol all_symbols;  // default-constructed = all zeros
    return read_range(all_symbols, start_time, end_time);
}

uint64_t TickStore::write(const TickRecord& record) {
    // Lazily create the writer on first write.
    if (!writer_) {
        writer_ = new BinaryWriter(path_);
    }

    uint64_t offset = writer_->write(record);

    // Keep the index in sync with newly written records.
    index_.add_entry(record.timestamp, offset);

    return offset;
}

void TickStore::flush() {
    if (writer_) {
        writer_->flush();
    }
}

void TickStore::close() {
    if (writer_) {
        writer_->close();
        delete writer_;
        writer_ = nullptr;
    }
}

uint64_t TickStore::rebuild_index() {
    // Close the writer first so all data is flushed.
    if (writer_) {
        writer_->close();
        delete writer_;
        writer_ = nullptr;
    }

    return index_.build(path_);
}

}  // namespace qf::data
