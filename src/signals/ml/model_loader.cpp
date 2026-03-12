#include "qf/signals/ml/model_loader.hpp"
#include <cstring>
#include <fstream>

namespace qf::signals {

bool ModelLoader::load(const std::string& path, LinearModel& model_out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error_ = "Failed to open file: " + path;
        return false;
    }

    // Read header
    ModelFileHeader header{};
    file.read(reinterpret_cast<char*>(&header.magic), sizeof(header.magic));
    file.read(reinterpret_cast<char*>(&header.version), sizeof(header.version));
    file.read(reinterpret_cast<char*>(&header.num_features), sizeof(header.num_features));

    if (!file) {
        error_ = "Failed to read header";
        return false;
    }

    if (header.magic != MAGIC) {
        error_ = "Invalid magic number";
        return false;
    }

    if (header.version != VERSION) {
        error_ = "Unsupported version: " + std::to_string(header.version);
        return false;
    }

    if (header.num_features == 0) {
        error_ = "num_features is zero";
        return false;
    }

    // Read bias
    double bias = 0.0;
    file.read(reinterpret_cast<char*>(&bias), sizeof(bias));
    if (!file) {
        error_ = "Failed to read bias";
        return false;
    }

    // Read weights
    std::vector<double> weights(header.num_features);
    file.read(reinterpret_cast<char*>(weights.data()),
              static_cast<std::streamsize>(header.num_features * sizeof(double)));
    if (!file) {
        error_ = "Failed to read weights";
        return false;
    }

    // Read lambda
    double lambda = 1.0;
    file.read(reinterpret_cast<char*>(&lambda), sizeof(lambda));
    if (!file) {
        error_ = "Failed to read lambda";
        return false;
    }

    // Construct model
    model_out = LinearModel(header.num_features, lambda);
    model_out.set_weights(weights);

    error_.clear();
    return true;
}

bool ModelLoader::save(const std::string& path, const LinearModel& model) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error_ = "Failed to open file for writing: " + path;
        return false;
    }

    // Write header
    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint32_t num_features = static_cast<uint32_t>(model.num_features());
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&num_features), sizeof(num_features));

    // Write bias
    double bias = model.bias();
    file.write(reinterpret_cast<const char*>(&bias), sizeof(bias));

    // Write weights
    file.write(reinterpret_cast<const char*>(model.weights().data()),
               static_cast<std::streamsize>(model.num_features() * sizeof(double)));

    // Write lambda
    double lambda = model.lambda();
    file.write(reinterpret_cast<const char*>(&lambda), sizeof(lambda));

    if (!file) {
        error_ = "Failed to write model file";
        return false;
    }

    error_.clear();
    return true;
}

}  // namespace qf::signals
