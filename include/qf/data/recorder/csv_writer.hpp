#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include "qf/data/data_types.hpp"
#include "qf/data/recorder/binary_writer.hpp"

namespace qf::data {

// CsvWriter — exports tick data to CSV format.
//
// Columns: timestamp, symbol, type, price, quantity, order_id
//
// Can either:
//   1. Write individual TickRecords directly to CSV.
//   2. Convert an entire binary .qfbt file to CSV.
class CsvWriter {
public:
    // Open a CSV file for writing.
    explicit CsvWriter(const std::string& path);

    ~CsvWriter();

    CsvWriter(const CsvWriter&) = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;

    // Write a single tick record as a CSV row.
    void write(const TickRecord& record);

    // Flush buffered output.
    void flush();

    // Close the file.
    void close();

    bool is_open() const { return file_ != nullptr; }

    uint64_t rows_written() const { return rows_written_; }

    // Static: convert a binary .qfbt file to CSV.
    // Returns the number of records converted.
    static uint64_t convert(const std::string& binary_path,
                            const std::string& csv_path);

private:
    void write_header();

    FILE*       file_{nullptr};
    std::string path_;
    uint64_t    rows_written_{0};
};

}  // namespace qf::data
