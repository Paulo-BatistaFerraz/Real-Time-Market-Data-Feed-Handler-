#include "qf/common/logging.hpp"
#include "qf/common/config.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <algorithm>
#include <mutex>

namespace qf {

std::shared_ptr<spdlog::logger> Logger::logger_;

// Default pattern: timestamp with μs precision, thread name, level, message
// Example: [2026-03-11 17:02:36.123456] [thread_name] [info] [NETWORK] connected
static constexpr const char* DEFAULT_PATTERN =
    "[%Y-%m-%d %H:%M:%S.%f] [%t] [%^%l%$] %v";

static spdlog::level::level_enum parse_level(const std::string& level_str) {
    std::string lower = level_str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "trace")    return spdlog::level::trace;
    if (lower == "debug")    return spdlog::level::debug;
    if (lower == "info")     return spdlog::level::info;
    if (lower == "warn" || lower == "warning") return spdlog::level::warn;
    if (lower == "error" || lower == "err")    return spdlog::level::err;
    if (lower == "critical") return spdlog::level::critical;
    if (lower == "off")      return spdlog::level::off;
    return spdlog::level::info;  // safe default
}

void Logger::init(const Config& config) {
    std::string level = config.has("logging.level")
        ? config.get<std::string>("logging.level")
        : "info";
    std::string file_path = config.has("logging.file")
        ? config.get<std::string>("logging.file")
        : "";
    std::string pattern = config.has("logging.pattern")
        ? config.get<std::string>("logging.pattern")
        : "";
    init(level, file_path, pattern);
}

void Logger::init(const std::string& level,
                  const std::string& file_path,
                  const std::string& pattern) {
    std::vector<spdlog::sink_ptr> sinks;

    // Console sink (color, stderr)
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    sinks.push_back(console_sink);

    // Optional file sink
    if (!file_path.empty()) {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            file_path, /*truncate=*/true);
        sinks.push_back(file_sink);
    }

    logger_ = std::make_shared<spdlog::logger>(
        "qf", sinks.begin(), sinks.end());

    // Pattern: μs precision timestamp + thread id + level + message
    std::string pat = pattern.empty() ? DEFAULT_PATTERN : pattern;
    logger_->set_pattern(pat);

    // Level
    logger_->set_level(parse_level(level));

    // Flush on warn+ for reliability
    logger_->flush_on(spdlog::level::warn);

    // Register as default
    spdlog::set_default_logger(logger_);
}

void Logger::set_level(const std::string& level) {
    ensure_default();
    logger_->set_level(parse_level(level));
}

std::shared_ptr<spdlog::logger>& Logger::get() {
    ensure_default();
    return logger_;
}

void Logger::ensure_default() {
    if (!logger_) {
        // Lazy fallback: console-only, info level
        init("info", "", "");
    }
}

}  // namespace qf
