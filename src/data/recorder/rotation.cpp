#include "qf/data/recorder/rotation.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#define mkdir_p(path) mkdir(path, 0755)
#endif

namespace qf::data {

Rotation::Rotation(const std::string& base_dir, uint32_t retention_days)
    : base_dir_(base_dir), retention_days_(retention_days) {}

std::string Rotation::current_date_str() const {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    return date_to_string(tm);
}

std::string Rotation::date_to_string(std::tm* tm) const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << (tm->tm_year + 1900) << '-'
        << std::setw(2) << (tm->tm_mon + 1) << '-'
        << std::setw(2) << tm->tm_mday;
    return oss.str();
}

std::string Rotation::current_path(const std::string& symbol) {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);

    last_day_ = tm->tm_mday;

    std::string date_str = date_to_string(tm);
    std::string dir = base_dir_ + "/" + date_str;
    ensure_directory(dir);

    return dir + "/" + symbol + ".bin";
}

std::string Rotation::rotate(const std::string& symbol) {
    last_day_ = -1;  // force re-evaluation
    return current_path(symbol);
}

bool Rotation::needs_rotation() const {
    if (last_day_ < 0) return true;

    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    return tm->tm_mday != last_day_;
}

void Rotation::ensure_directory(const std::string& path) {
    // Create base_dir first, then the date subdirectory
    mkdir_p(base_dir_.c_str());
    mkdir_p(path.c_str());
}

uint32_t Rotation::cleanup() {
    if (retention_days_ == 0) return 0;

    uint32_t removed = 0;

    // Calculate cutoff date
    std::time_t now = std::time(nullptr);
    std::time_t cutoff = now - static_cast<std::time_t>(retention_days_) * 86400;

#ifdef _WIN32
    // Windows directory enumeration
    WIN32_FIND_DATAA find_data;
    std::string search_path = base_dir_ + "/*";
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &find_data);

    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;

        std::string name = find_data.cFileName;
        if (name == "." || name == "..") continue;

        // Parse YYYY-MM-DD directory name
        if (name.size() != 10 || name[4] != '-' || name[7] != '-') continue;

        std::tm dir_tm{};
        std::istringstream iss(name);
        char dash;
        int year, month, day;
        iss >> year >> dash >> month >> dash >> day;
        if (iss.fail()) continue;

        dir_tm.tm_year = year - 1900;
        dir_tm.tm_mon = month - 1;
        dir_tm.tm_mday = day;
        dir_tm.tm_isdst = -1;

        std::time_t dir_time = std::mktime(&dir_tm);
        if (dir_time < cutoff) {
            // Remove all files in directory, then the directory itself
            std::string dir_path = base_dir_ + "/" + name;
            std::string file_search = dir_path + "/*";
            WIN32_FIND_DATAA file_data;
            HANDLE hFileFind = FindFirstFileA(file_search.c_str(), &file_data);

            if (hFileFind != INVALID_HANDLE_VALUE) {
                do {
                    std::string fname = file_data.cFileName;
                    if (fname != "." && fname != "..") {
                        std::string file_path = dir_path + "/" + fname;
                        DeleteFileA(file_path.c_str());
                    }
                } while (FindNextFileA(hFileFind, &file_data));
                FindClose(hFileFind);
            }

            if (RemoveDirectoryA(dir_path.c_str())) {
                ++removed;
            }
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
#else
    DIR* dir = opendir(base_dir_.c_str());
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        // Parse YYYY-MM-DD directory name
        if (name.size() != 10 || name[4] != '-' || name[7] != '-') continue;

        std::tm dir_tm{};
        std::istringstream iss(name);
        char dash;
        int year, month, day;
        iss >> year >> dash >> month >> dash >> day;
        if (iss.fail()) continue;

        dir_tm.tm_year = year - 1900;
        dir_tm.tm_mon = month - 1;
        dir_tm.tm_mday = day;
        dir_tm.tm_isdst = -1;

        std::time_t dir_time = std::mktime(&dir_tm);
        if (dir_time < cutoff) {
            std::string dir_path = base_dir_ + "/" + name;

            // Remove files in directory
            DIR* sub = opendir(dir_path.c_str());
            if (sub) {
                struct dirent* sub_entry;
                while ((sub_entry = readdir(sub)) != nullptr) {
                    std::string fname = sub_entry->d_name;
                    if (fname != "." && fname != "..") {
                        std::string file_path = dir_path + "/" + fname;
                        std::remove(file_path.c_str());
                    }
                }
                closedir(sub);
            }

            if (rmdir(dir_path.c_str()) == 0) {
                ++removed;
            }
        }
    }

    closedir(dir);
#endif

    return removed;
}

}  // namespace qf::data
