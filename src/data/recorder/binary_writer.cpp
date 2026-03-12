#include "qf/data/recorder/binary_writer.hpp"

#include <stdexcept>
#include <cstring>
#include <utility>

namespace qf::data {

BinaryWriter::BinaryWriter(const std::string& path, size_t flush_count)
    : path_(path)
    , flush_count_(flush_count)
{
    buffer_.reserve(flush_count_);

    file_ = std::fopen(path_.c_str(), "wb");
    if (!file_) {
        throw std::runtime_error("BinaryWriter: failed to open file: " + path_);
    }

    // Write initial header (record_count = 0, updated on close).
    FileHeader hdr;
    if (std::fwrite(&hdr, sizeof(FileHeader), 1, file_) != 1) {
        std::fclose(file_);
        file_ = nullptr;
        throw std::runtime_error("BinaryWriter: failed to write header: " + path_);
    }

    next_offset_ = sizeof(FileHeader);
}

BinaryWriter::~BinaryWriter() {
    if (file_) {
        close();
    }
}

BinaryWriter::BinaryWriter(BinaryWriter&& other) noexcept
    : file_(other.file_)
    , path_(std::move(other.path_))
    , flush_count_(other.flush_count_)
    , record_count_(other.record_count_)
    , next_offset_(other.next_offset_)
    , buffer_(std::move(other.buffer_))
{
    other.file_ = nullptr;
    other.record_count_ = 0;
    other.next_offset_ = 0;
}

BinaryWriter& BinaryWriter::operator=(BinaryWriter&& other) noexcept {
    if (this != &other) {
        if (file_) {
            close();
        }
        file_         = other.file_;
        path_         = std::move(other.path_);
        flush_count_  = other.flush_count_;
        record_count_ = other.record_count_;
        next_offset_  = other.next_offset_;
        buffer_       = std::move(other.buffer_);

        other.file_ = nullptr;
        other.record_count_ = 0;
        other.next_offset_ = 0;
    }
    return *this;
}

uint64_t BinaryWriter::write(const TickRecord& record) {
    if (!file_) {
        throw std::runtime_error("BinaryWriter: file is not open");
    }

    // Compute the offset this record will occupy once flushed.
    uint64_t offset = next_offset_ + buffer_.size() * sizeof(TickRecord);

    buffer_.push_back(record);
    ++record_count_;

    if (buffer_.size() >= flush_count_) {
        flush_buffer();
    }

    return offset;
}

void BinaryWriter::flush() {
    if (!file_) return;
    flush_buffer();
    std::fflush(file_);
}

void BinaryWriter::close() {
    if (!file_) return;

    flush_buffer();
    write_header();
    std::fclose(file_);
    file_ = nullptr;
}

void BinaryWriter::flush_buffer() {
    if (buffer_.empty() || !file_) return;

    size_t written = std::fwrite(buffer_.data(),
                                 sizeof(TickRecord),
                                 buffer_.size(),
                                 file_);
    if (written != buffer_.size()) {
        throw std::runtime_error("BinaryWriter: incomplete write to " + path_);
    }

    next_offset_ += buffer_.size() * sizeof(TickRecord);
    buffer_.clear();
}

void BinaryWriter::write_header() {
    if (!file_) return;

    // Seek to the beginning and overwrite the header with final record count.
    std::fseek(file_, 0, SEEK_SET);

    FileHeader hdr;
    hdr.record_count = record_count_;

    std::fwrite(&hdr, sizeof(FileHeader), 1, file_);

    // Seek back to end.
    std::fseek(file_, 0, SEEK_END);
}

}  // namespace qf::data
