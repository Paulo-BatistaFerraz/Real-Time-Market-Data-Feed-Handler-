#pragma once

#include <cstdint>
#include <string>
#include <ctime>

namespace qf::data {

// Rotation — manages daily file rotation for tick data.
// Files are organized as: base_dir/YYYY-MM-DD/symbol.bin
// Supports automatic midnight rotation and retention-based cleanup.
class Rotation {
public:
    // base_dir: root directory for data files (e.g. "data")
    // retention_days: files older than this are cleaned up (0 = no cleanup)
    explicit Rotation(const std::string& base_dir, uint32_t retention_days = 30);

    // Get the current file path for a symbol, creating directories as needed.
    // Returns path like "data/2026-03-12/AAPL.bin"
    std::string current_path(const std::string& symbol);

    // Force a rotation — returns the new path for the symbol.
    std::string rotate(const std::string& symbol);

    // Check if a rotation is needed (i.e., date has changed since last check).
    bool needs_rotation() const;

    // Clean up files older than retention_days. Returns number of directories removed.
    uint32_t cleanup();

    // Get the base directory.
    const std::string& base_dir() const { return base_dir_; }

    // Get current date string (YYYY-MM-DD).
    std::string current_date_str() const;

private:
    void ensure_directory(const std::string& path);
    std::string date_to_string(std::tm* tm) const;

    std::string base_dir_;
    uint32_t    retention_days_;
    int         last_day_{-1};  // day-of-month when last path was generated
};

}  // namespace qf::data
