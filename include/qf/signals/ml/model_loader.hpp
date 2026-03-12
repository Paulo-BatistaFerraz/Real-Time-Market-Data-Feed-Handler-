#pragma once

#include <cstdint>
#include <string>
#include "qf/signals/ml/linear_model.hpp"

namespace qf::signals {

// Binary model file format:
//   Offset  Size   Description
//   0       4      Magic number: 0x4D4C4D44 ("MLMD")
//   4       4      Version (uint32_t, currently 1)
//   8       4      Number of features (uint32_t)
//   12      8      Bias / intercept (double)
//   20      N*8    Weights array (N doubles, little-endian)
//   20+N*8  8      Lambda / regularization (double)

struct ModelFileHeader {
    uint32_t magic       = 0x4D4C4D44;  // "MLMD"
    uint32_t version     = 1;
    uint32_t num_features = 0;
};

// ModelLoader — loads and saves pre-trained LinearModel weights from/to
// a compact binary file.
class ModelLoader {
public:
    ModelLoader() = default;

    // Load a LinearModel from a binary file. Returns true on success.
    // On failure, model is left unchanged and an error message is set.
    bool load(const std::string& path, LinearModel& model_out);

    // Save a LinearModel to a binary file. Returns true on success.
    bool save(const std::string& path, const LinearModel& model);

    // Last error message (empty if no error).
    const std::string& error() const { return error_; }

private:
    std::string error_;

    static constexpr uint32_t MAGIC   = 0x4D4C4D44;
    static constexpr uint32_t VERSION = 1;
};

}  // namespace qf::signals
