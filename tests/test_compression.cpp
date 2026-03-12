#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <string>
#include "qf/data/recorder/compression.hpp"
#include "qf/data/data_types.hpp"

using namespace qf::data;

// --- LZ4 compress → decompress round-trip ---

TEST(CompressionTest, BasicRoundTrip) {
    const std::string input = "Hello, QuantFlow compression test!";

    size_t max_out = Compression::max_compressed_size(input.size());
    std::vector<uint8_t> compressed(max_out);

    size_t compressed_size = Compression::compress(
        input.data(), input.size(),
        compressed.data(), compressed.size());
    ASSERT_GT(compressed_size, 0u);

    std::vector<uint8_t> decompressed(input.size());
    size_t decompressed_size = Compression::decompress(
        compressed.data(), compressed_size,
        decompressed.data(), decompressed.size());

    ASSERT_EQ(decompressed_size, input.size());
    EXPECT_EQ(std::memcmp(decompressed.data(), input.data(), input.size()), 0);
}

TEST(CompressionTest, TickRecordArrayRoundTrip) {
    // Create an array of TickRecords with known data.
    constexpr int N = 50;
    std::vector<TickRecord> records(N);

    for (int i = 0; i < N; ++i) {
        records[i].timestamp = static_cast<uint64_t>(i) * 1'000'000;
        records[i].message_type = 'A';
        records[i].payload_length = 8;
        for (int j = 0; j < 8; ++j) {
            records[i].payload[j] = static_cast<uint8_t>((i + j) & 0xFF);
        }
    }

    size_t input_size = N * sizeof(TickRecord);
    size_t max_out = Compression::max_compressed_size(input_size);
    std::vector<uint8_t> compressed(max_out);

    size_t compressed_size = Compression::compress(
        records.data(), input_size,
        compressed.data(), compressed.size());
    ASSERT_GT(compressed_size, 0u);

    // Decompress back into TickRecord array.
    std::vector<TickRecord> restored(N);
    size_t decompressed_size = Compression::decompress(
        compressed.data(), compressed_size,
        restored.data(), input_size);

    ASSERT_EQ(decompressed_size, input_size);

    // Verify every field of every record.
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(restored[i].timestamp, records[i].timestamp) << "record " << i;
        EXPECT_EQ(restored[i].message_type, records[i].message_type) << "record " << i;
        EXPECT_EQ(restored[i].payload_length, records[i].payload_length) << "record " << i;
        EXPECT_EQ(std::memcmp(restored[i].payload, records[i].payload,
                              records[i].payload_length), 0)
            << "record " << i;
    }
}

TEST(CompressionTest, CompressedSmallerForRepetitive) {
    // Highly repetitive data should compress well.
    std::vector<uint8_t> data(4096, 0xAA);

    size_t max_out = Compression::max_compressed_size(data.size());
    std::vector<uint8_t> compressed(max_out);

    size_t compressed_size = Compression::compress(
        data.data(), data.size(),
        compressed.data(), compressed.size());

    ASSERT_GT(compressed_size, 0u);
    EXPECT_LT(compressed_size, data.size());
}

TEST(CompressionTest, LargeDataRoundTrip) {
    // 1 MB of pseudo-random data.
    constexpr size_t SIZE = 1024 * 1024;
    std::vector<uint8_t> input(SIZE);
    for (size_t i = 0; i < SIZE; ++i) {
        input[i] = static_cast<uint8_t>((i * 31 + 17) & 0xFF);
    }

    size_t max_out = Compression::max_compressed_size(SIZE);
    std::vector<uint8_t> compressed(max_out);

    size_t compressed_size = Compression::compress(
        input.data(), SIZE,
        compressed.data(), compressed.size());
    ASSERT_GT(compressed_size, 0u);

    std::vector<uint8_t> decompressed(SIZE);
    size_t decompressed_size = Compression::decompress(
        compressed.data(), compressed_size,
        decompressed.data(), decompressed.size());

    ASSERT_EQ(decompressed_size, SIZE);
    EXPECT_EQ(std::memcmp(decompressed.data(), input.data(), SIZE), 0);
}

TEST(CompressionTest, NullInputReturnsZero) {
    uint8_t buf[64];
    EXPECT_EQ(Compression::compress(nullptr, 10, buf, 64), 0u);
    EXPECT_EQ(Compression::decompress(nullptr, 10, buf, 64), 0u);
}

TEST(CompressionTest, ZeroSizeReturnsZero) {
    uint8_t buf[64] = {};
    EXPECT_EQ(Compression::compress(buf, 0, buf, 64), 0u);
    EXPECT_EQ(Compression::decompress(buf, 0, buf, 64), 0u);
}

TEST(CompressionTest, MaxCompressedSizePositive) {
    EXPECT_GT(Compression::max_compressed_size(100), 0u);
    EXPECT_GT(Compression::max_compressed_size(1), 0u);
    EXPECT_GT(Compression::max_compressed_size(1024 * 1024), 0u);
}
