#include "utils/Logger.h"

#include <cstdlib>
#include <iostream>

namespace studio {

Logger& Logger::GetInstance() {
    static Logger inst;
    return inst;
}

std::filesystem::path Logger::DefaultLogFile() {
    std::filesystem::path base;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA")) {
        base = std::filesystem::path(appdata) / "KimodoStudio" / "logs";
    } else {
        base = std::filesystem::path("logs");
    }
#else
    if (const char* home = std::getenv("HOME")) {
        base = std::filesystem::path(home) / ".kimodo-studio" / "logs";
    } else {
        base = std::filesystem::path("logs");
    }
#endif
    return base / "kimodo-studio.log";
}

void Logger::Init(const std::filesystem::path& file) {
    std::lock_guard<std::mutex> lock(mutex);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    stream.open(file, std::ios::app);
    // Never fails hard: console fallback always works.
}

const char* Logger::LevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARNING";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

void Logger::Log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex);
    std::string line = std::string("[") + LevelName(level) + "] " + msg + "\n";
    std::cout << line;
    if (stream.is_open()) {
        stream << line;
        stream.flush();
    }
}

} // namespace studio
