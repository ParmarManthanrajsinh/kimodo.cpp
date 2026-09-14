#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace studio {

enum class LogLevel { Trace, Debug, Info, Warning, Error };

class Logger {
public:
    static Logger& instance();

    void init(const std::filesystem::path& file);
    void log(LogLevel level, const std::string& msg);

    void trace(const std::string& msg) { log(LogLevel::Trace, msg); }
    void debug(const std::string& msg) { log(LogLevel::Debug, msg); }
    void info(const std::string& msg) { log(LogLevel::Info, msg); }
    void warning(const std::string& msg) { log(LogLevel::Warning, msg); }
    void error(const std::string& msg) { log(LogLevel::Error, msg); }

    static std::filesystem::path defaultLogFile();

private:
    Logger() = default;
    static const char* levelName(LogLevel level);

    std::mutex mutex_;
    std::ofstream stream_;
};

} // namespace studio
