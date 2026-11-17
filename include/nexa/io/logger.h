#pragma once
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

namespace nexa {

// Appends timestamped lines to a TXT log, e.g. logs/operations.log
class Logger {
    std::string path_;
public:
    explicit Logger(std::string path) : path_(std::move(path)) {}

    void log(const std::string& level, const std::string& msg) const {
        std::filesystem::path p(path_);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        std::ofstream out(path_, std::ios::app);
        if (!out) return;
        std::time_t t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        out << '[' << buf << "] [" << level << "] " << msg << "\n";
    }
    void info(const std::string& m) const { log("INFO", m); }
    void warn(const std::string& m) const { log("WARN", m); }
    void error(const std::string& m) const { log("ERROR", m); }
};

}  // namespace nexa
