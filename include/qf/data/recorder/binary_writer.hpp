#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "qf/data/data_types.hpp"

namespace qf::data {

// Binary file layout:
//   [FileHeader]            — fixed-size header
//   [TickRecord] * N        — sequential tick records
//
// The header is updated on close() with the final record count.

#pragma pack(push, 1)
struct FileHeader {
    uint32_t magic;           // 0x51464254 ("QFBT" — QuantFlow Binary Ticks)
    uint16_t version;         // format version (currently 1)
    uint64_t record_count;    // total records written
    uint64_t created_ns;      // file creation timestamp (nanos since midnight)

    FileHeader() : magic{0x51464254}, version{1},
                   record_count{0}, created_ns{0} {}
};
#pragma pack(pop)

static constexpr uint32_t BINARY_FILE_MAGIC   = 0x51464254;
static constexpr uint16_t BINARY_FILE_VERSION = 1;
static constexpr size_t   DEFAULT_FLUSH_COUNT = 256;

// BinaryWriter — buffered binary writer for TickRecords.
// Writes records to a file with a FileHeader, flushing every N records.
class BinaryWriter {
public:
    // Open a file for writing. flush_count = records buffered before auto-flush.
    explicit BinaryWriter(const std::string& path,
                          size_t flush_count = DEFAULT_FLUSH_COUNT);

    ~BinaryWriter();

    // Not copyable, movable.
    BinaryWriter(const BinaryWriter&) = delete;
    BinaryWriter& operator=(const BinaryWriter&) = delete;
    BinaryWriter(BinaryWriter&& other) noexcept;
    BinaryWriter& operator=(BinaryWriter&& other) noexcept;

    // Append a tick record. Returns the byte offset of the written record
    // within the file (offset from start of file, past the header).
    uint64_t write(const TickRecord& record);

    // Flush buffered records to disk.
    void flush();

    // Flush remaining records, update header, and close the file.
    void close();

    // Number of records written (including buffered, not yet flushed).
    uint64_t record_count() const { return record_count_; }

    // Whether the file is open and ready for writing.
    bool is_open() const { return file_ != nullptr; }

private:
    void flush_buffer();
    void write_header();

    FILE*                    file_{nullptr};
    std::string              path_;
    size_t                   flush_count_;
    uint64_t                 record_count_{0};
    uint64_t                 next_offset_{0};  // byte offset for next record
    std::vector<TickRecord>  buffer_;
};

}  // namespace qf::data
