#include "qf/data/recorder/compression.hpp"
#include <lz4.h>

namespace qf::data {

size_t Compression::compress(const void* input, size_t input_size,
                             void* output, size_t output_capacity) {
    if (!input || !output || input_size == 0 || output_capacity == 0) {
        return 0;
    }

    int result = LZ4_compress_default(
        static_cast<const char*>(input),
        static_cast<char*>(output),
        static_cast<int>(input_size),
        static_cast<int>(output_capacity)
    );

    return (result > 0) ? static_cast<size_t>(result) : 0;
}

size_t Compression::decompress(const void* input, size_t input_size,
                               void* output, size_t output_capacity) {
    if (!input || !output || input_size == 0 || output_capacity == 0) {
        return 0;
    }

    int result = LZ4_decompress_safe(
        static_cast<const char*>(input),
        static_cast<char*>(output),
        static_cast<int>(input_size),
        static_cast<int>(output_capacity)
    );

    return (result > 0) ? static_cast<size_t>(result) : 0;
}

size_t Compression::max_compressed_size(size_t input_size) {
    return static_cast<size_t>(LZ4_compressBound(static_cast<int>(input_size)));
}

}  // namespace qf::data
