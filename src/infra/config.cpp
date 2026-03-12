#include "qf/common/config.hpp"
#include <yaml-cpp/yaml.h>
#include <algorithm>

namespace qf {

// --- Internal helpers ---

static void flatten(const YAML::Node& node, const std::string& prefix,
                    std::map<std::string, std::string>& out) {
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            std::string key = it->first.as<std::string>();
            std::string full = prefix.empty() ? key : prefix + "." + key;
            flatten(it->second, full, out);
        }
    } else if (node.IsSequence()) {
        // Store sequences as comma-separated values
        std::string val;
        for (std::size_t i = 0; i < node.size(); ++i) {
            if (i > 0) val += ",";
            val += node[i].as<std::string>();
        }
        out[prefix] = val;
    } else if (node.IsScalar()) {
        out[prefix] = node.as<std::string>();
    }
}

static std::map<std::string, std::string> flatten_root(const YAML::Node& root) {
    std::map<std::string, std::string> flat;
    flatten(root, "", flat);
    return flat;
}

// --- Config public interface ---

Config Config::load(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Config::load failed for '" + path + "': " + e.what());
    }
    return Config(flatten_root(root));
}

Config Config::parse(const std::string& yaml_str) {
    YAML::Node root;
    try {
        root = YAML::Load(yaml_str);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error(std::string("Config::parse failed: ") + e.what());
    }
    return Config(flatten_root(root));
}

bool Config::has(const std::string& key) const {
    return values_.find(key) != values_.end();
}

std::vector<std::string> Config::keys(const std::string& prefix) const {
    std::vector<std::string> result;
    std::string match = prefix.empty() ? "" : prefix + ".";
    for (auto& kv : values_) {
        if (prefix.empty()) {
            auto dot = kv.first.find('.');
            std::string top = (dot == std::string::npos) ? kv.first : kv.first.substr(0, dot);
            if (result.empty() || result.back() != top) {
                result.push_back(top);
            }
        } else if (kv.first.compare(0, match.size(), match) == 0) {
            std::string rest = kv.first.substr(match.size());
            auto dot = rest.find('.');
            std::string child = (dot == std::string::npos) ? rest : rest.substr(0, dot);
            if (result.empty() || result.back() != child) {
                result.push_back(child);
            }
        }
    }
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

const std::string& Config::raw(const std::string& key) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
        throw std::runtime_error("Config key not found: " + key);
    }
    return it->second;
}

const std::string* Config::try_raw(const std::string& key) const {
    auto it = values_.find(key);
    if (it == values_.end()) return nullptr;
    return &it->second;
}

// --- Template specializations ---

template <>
std::string Config::get<std::string>(const std::string& key) const {
    return raw(key);
}

template <>
std::string Config::get<std::string>(const std::string& key, const std::string& default_value) const {
    auto* v = try_raw(key);
    return v ? *v : default_value;
}

template <>
int Config::get<int>(const std::string& key) const {
    return std::stoi(raw(key));
}

template <>
int Config::get<int>(const std::string& key, const int& default_value) const {
    auto* v = try_raw(key);
    return v ? std::stoi(*v) : default_value;
}

template <>
uint16_t Config::get<uint16_t>(const std::string& key) const {
    return static_cast<uint16_t>(std::stoul(raw(key)));
}

template <>
uint16_t Config::get<uint16_t>(const std::string& key, const uint16_t& default_value) const {
    auto* v = try_raw(key);
    return v ? static_cast<uint16_t>(std::stoul(*v)) : default_value;
}

template <>
uint32_t Config::get<uint32_t>(const std::string& key) const {
    return static_cast<uint32_t>(std::stoul(raw(key)));
}

template <>
uint32_t Config::get<uint32_t>(const std::string& key, const uint32_t& default_value) const {
    auto* v = try_raw(key);
    return v ? static_cast<uint32_t>(std::stoul(*v)) : default_value;
}

template <>
uint64_t Config::get<uint64_t>(const std::string& key) const {
    return std::stoull(raw(key));
}

template <>
uint64_t Config::get<uint64_t>(const std::string& key, const uint64_t& default_value) const {
    auto* v = try_raw(key);
    return v ? std::stoull(*v) : default_value;
}

template <>
double Config::get<double>(const std::string& key) const {
    return std::stod(raw(key));
}

template <>
double Config::get<double>(const std::string& key, const double& default_value) const {
    auto* v = try_raw(key);
    return v ? std::stod(*v) : default_value;
}

template <>
bool Config::get<bool>(const std::string& key) const {
    const std::string& v = raw(key);
    return v == "true" || v == "1" || v == "yes";
}

template <>
bool Config::get<bool>(const std::string& key, const bool& default_value) const {
    auto* v = try_raw(key);
    if (!v) return default_value;
    return *v == "true" || *v == "1" || *v == "yes";
}

}  // namespace qf
