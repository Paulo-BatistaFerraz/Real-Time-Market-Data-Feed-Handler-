#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <string>
#include <memory>

namespace qf {

class Config;  // forward declaration

/// Structured logging facade wrapping spdlog.
/// Call Logger::init() once at startup; then use QF_LOG_* macros everywhere.
class Logger {
public:
    /// Initialize the global logger from a Config object.
    /// Reads keys:  logging.level        (default: "info")
    ///              logging.file         (optional file sink path)
    ///              logging.pattern      (optional pattern override)
    static void init(const Config& config);

    /// Initialize with explicit parameters (for use without Config).
    static void init(const std::string& level = "info",
                     const std::string& file_path = "",
                     const std::string& pattern = "");

    /// Set the log level at runtime.
    static void set_level(const std::string& level);

    /// Get the underlying spdlog logger.
    static std::shared_ptr<spdlog::logger>& get();

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static void ensure_default();
};

}  // namespace qf

// --- Convenience macros with level-check guard (zero cost on hot path) ---

#define QF_LOG_DEBUG(...)                                           \
    do {                                                            \
        auto& lg_ = ::qf::Logger::get();                           \
        if (lg_->should_log(spdlog::level::debug))                 \
            lg_->debug(__VA_ARGS__);                                \
    } while (0)

#define QF_LOG_INFO(...)                                            \
    do {                                                            \
        auto& lg_ = ::qf::Logger::get();                           \
        if (lg_->should_log(spdlog::level::info))                  \
            lg_->info(__VA_ARGS__);                                 \
    } while (0)

#define QF_LOG_WARN(...)                                            \
    do {                                                            \
        auto& lg_ = ::qf::Logger::get();                           \
        if (lg_->should_log(spdlog::level::warn))                  \
            lg_->warn(__VA_ARGS__);                                 \
    } while (0)

#define QF_LOG_ERROR(...)                                           \
    do {                                                            \
        auto& lg_ = ::qf::Logger::get();                           \
        if (lg_->should_log(spdlog::level::err))                   \
            lg_->error(__VA_ARGS__);                                \
    } while (0)

// Subsystem-tagged variants: QF_LOG_INFO_T("NETWORK", "listening on port {}", port)
// Prepends [TAG] to the format string for structured subsystem identification.

#define QF_LOG_DEBUG_T(tag, fmt, ...)                               \
    QF_LOG_DEBUG("[" tag "] " fmt, ##__VA_ARGS__)

#define QF_LOG_INFO_T(tag, fmt, ...)                                \
    QF_LOG_INFO("[" tag "] " fmt, ##__VA_ARGS__)

#define QF_LOG_WARN_T(tag, fmt, ...)                                \
    QF_LOG_WARN("[" tag "] " fmt, ##__VA_ARGS__)

#define QF_LOG_ERROR_T(tag, fmt, ...)                               \
    QF_LOG_ERROR("[" tag "] " fmt, ##__VA_ARGS__)
