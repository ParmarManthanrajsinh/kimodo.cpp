#include "utils/Logger.h"

#include <cstdlib>
#include <iostream>

namespace studio {

FLogger& FLogger::GetInstance() {
    static FLogger inst;
    return inst;
}

std::filesystem::path FLogger::DefaultLogFile() {
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

void FLogger::Init(const std::filesystem::path& file) {
    std::lock_guard<std::mutex> lock(mutex);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    stream.open(file, std::ios::app);
    // Never fails hard: console fallback always works.
}

const char* FLogger::LevelName(ELogLevel level) {
    switch (level) {
        case ELogLevel::Trace: return "TRACE";
        case ELogLevel::Debug: return "DEBUG";
        case ELogLevel::Info: return "INFO";
        case ELogLevel::Warning: return "WARNING";
        case ELogLevel::Error: return "ERROR";
    }
    return "INFO";
}

void FLogger::Log(ELogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex);
    std::string line = std::string("[") + LevelName(level) + "] " + msg + "\n";
    std::cout << line;
    if (stream.is_open()) {
        stream << line;
        stream.flush();
    }
}

} // namespace studio
