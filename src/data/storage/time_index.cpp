#include "qf/data/storage/time_index.hpp"
#include "qf/data/data_types.hpp"
#include "qf/data/recorder/binary_writer.hpp"
#include <algorithm>
#include <cstdio>

namespace qf::data {

uint64_t TimeIndex::build(const std::string& tick_file_path) {
    entries_.clear();

    FILE* file = std::fopen(tick_file_path.c_str(), "rb");
    if (!file) {
        return 0;
    }

    // Read and validate the file header.
    FileHeader header;
    if (std::fread(&header, sizeof(FileHeader), 1, file) != 1) {
        std::fclose(file);
        return 0;
    }

    if (header.magic != BINARY_FILE_MAGIC) {
        std::fclose(file);
        return 0;
    }

    // Reserve space based on expected record count.
    if (header.record_count > 0) {
        entries_.reserve(static_cast<size_t>(header.record_count));
    }

    // Scan through all records, recording (timestamp, offset) for each.
    uint64_t offset = sizeof(FileHeader);
    TickRecord record;
    while (std::fread(&record, sizeof(TickRecord), 1, file) == 1) {
        entries_.push_back({record.timestamp, offset});
        offset += sizeof(TickRecord);
    }

    std::fclose(file);

    // Ensure entries are sorted by timestamp (should already be in order
    // for well-formed files, but sort defensively).
    std::sort(entries_.begin(), entries_.end());

    return entries_.size();
}

uint64_t TimeIndex::find(Timestamp target) const {
    if (entries_.empty()) {
        return 0;
    }

    IndexEntry search_key;
    search_key.timestamp = target;
    search_key.offset = 0;

    auto it = std::lower_bound(entries_.begin(), entries_.end(), search_key);

    if (it == entries_.end()) {
        return 0;
    }

    return it->offset;
}

uint64_t TimeIndex::upper_bound(Timestamp target) const {
    if (entries_.empty()) {
        return 0;
    }

    IndexEntry search_key;
    search_key.timestamp = target;
    search_key.offset = 0;

    auto it = std::upper_bound(entries_.begin(), entries_.end(), search_key);

    if (it == entries_.end()) {
        return 0;
    }

    return it->offset;
}

// Index file format:
//   [uint64_t entry_count]
//   [IndexEntry] * entry_count
bool TimeIndex::save(const std::string& index_path) const {
    FILE* file = std::fopen(index_path.c_str(), "wb");
    if (!file) {
        return false;
    }

    uint64_t count = entries_.size();
    if (std::fwrite(&count, sizeof(count), 1, file) != 1) {
        std::fclose(file);
        return false;
    }

    if (!entries_.empty()) {
        size_t written = std::fwrite(entries_.data(),
                                     sizeof(IndexEntry),
                                     entries_.size(),
                                     file);
        if (written != entries_.size()) {
            std::fclose(file);
            return false;
        }
    }

    std::fclose(file);
    return true;
}

bool TimeIndex::load(const std::string& index_path) {
    entries_.clear();

    FILE* file = std::fopen(index_path.c_str(), "rb");
    if (!file) {
        return false;
    }

    uint64_t count = 0;
    if (std::fread(&count, sizeof(count), 1, file) != 1) {
        std::fclose(file);
        return false;
    }

    if (count > 0) {
        entries_.resize(static_cast<size_t>(count));
        size_t read = std::fread(entries_.data(),
                                 sizeof(IndexEntry),
                                 static_cast<size_t>(count),
                                 file);
        if (read != static_cast<size_t>(count)) {
            entries_.clear();
            std::fclose(file);
            return false;
        }
    }

    std::fclose(file);
    return true;
}

}  // namespace qf::data
