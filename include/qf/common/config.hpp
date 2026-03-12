#pragma once

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace qf {

/// YAML-based hierarchical configuration loader.
/// Supports dot-notation access (e.g., "feed.multicast_address")
/// and typed value retrieval with optional defaults.
class Config {
public:
    /// Load configuration from a YAML file.
    /// Throws std::runtime_error on file read or parse failure.
    static Config load(const std::string& path);

    /// Parse configuration from a YAML string (for testing).
    static Config parse(const std::string& yaml_str);

    /// Get a typed value by dot-notation key.
    /// Throws std::runtime_error if key not found.
    template <typename T>
    T get(const std::string& key) const;

    /// Get a typed value by dot-notation key, returning default if missing.
    template <typename T>
    T get(const std::string& key, const T& default_value) const;

    /// Check if a key exists.
    bool has(const std::string& key) const;

    /// Get all keys under a given prefix (one level deep).
    std::vector<std::string> keys(const std::string& prefix = "") const;

private:
    explicit Config(std::map<std::string, std::string> values)
        : values_(std::move(values)) {}

    // Flat key-value store: "feed.multicast_address" -> "239.255.0.1"
    std::map<std::string, std::string> values_;

    // Internal: get raw string value, throws if missing
    const std::string& raw(const std::string& key) const;

    // Internal: try to get raw string value, returns nullptr if missing
    const std::string* try_raw(const std::string& key) const;
};

// --- Template specialization declarations (defined in config.cpp) ---

template <>
std::string Config::get<std::string>(const std::string& key) const;

template <>
std::string Config::get<std::string>(const std::string& key, const std::string& default_value) const;

template <>
int Config::get<int>(const std::string& key) const;

template <>
int Config::get<int>(const std::string& key, const int& default_value) const;

template <>
uint16_t Config::get<uint16_t>(const std::string& key) const;

template <>
uint16_t Config::get<uint16_t>(const std::string& key, const uint16_t& default_value) const;

template <>
uint32_t Config::get<uint32_t>(const std::string& key) const;

template <>
uint32_t Config::get<uint32_t>(const std::string& key, const uint32_t& default_value) const;

template <>
uint64_t Config::get<uint64_t>(const std::string& key) const;

template <>
uint64_t Config::get<uint64_t>(const std::string& key, const uint64_t& default_value) const;

template <>
double Config::get<double>(const std::string& key) const;

template <>
double Config::get<double>(const std::string& key, const double& default_value) const;

template <>
bool Config::get<bool>(const std::string& key) const;

template <>
bool Config::get<bool>(const std::string& key, const bool& default_value) const;

}  // namespace qf
