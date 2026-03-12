#pragma once

#include <cstddef>
#include <cstdint>

namespace qf::data {

// Compression — wraps LZ4 block compression for tick data storage.
// Provides compress/decompress with raw byte buffers.
class Compression {
public:
    // Compress input into output buffer.
    // Returns the number of bytes written to output, or 0 on failure.
    static size_t compress(const void* input, size_t input_size,
                           void* output, size_t output_capacity);

    // Decompress input into output buffer.
    // Returns the number of bytes written to output, or 0 on failure.
    static size_t decompress(const void* input, size_t input_size,
                             void* output, size_t output_capacity);

    // Returns the maximum compressed size for a given input size.
    // Useful for pre-allocating output buffers.
    static size_t max_compressed_size(size_t input_size);
};

}  // namespace qf::data
