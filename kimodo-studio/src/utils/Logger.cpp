#include "utils/Logger.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>

namespace studio
{

Logger& Logger::GetInstance() noexcept
{
    static Logger inst;
    return inst;
}

std::filesystem::path Logger::DefaultLogFile()
{
    std::filesystem::path base;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA"))
    {
        base = std::filesystem::path(appdata) / "KimodoStudio" / "logs";
    }
    else
    {
        base = std::filesystem::path("logs");
    }
#else
    if (const char* home = std::getenv("HOME"))
    {
        base = std::filesystem::path(home) / ".kimodo-studio" / "logs";
    }
    else
    {
        base = std::filesystem::path("logs");
    }
#endif
    return base / "kimodo-studio.log";
}

void Logger::Init(const std::filesystem::path& file)
{
    std::lock_guard<std::mutex> lock(mutex);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    stream.open(file, std::ios::app);
    // Never fails hard: console fallback always works.
}

const char* Logger::LevelName(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

namespace
{

std::string LocalTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[20];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                  tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

} // namespace

void Logger::Log(LogLevel level, std::string_view msg)
{
    std::lock_guard<std::mutex> lock(mutex);
    // Timestamps are load-bearing for multi-attempt download forensics:
    // repeated identical lines without times cannot be correlated.
    std::string line = "[" + LocalTimestamp() + " " + LevelName(level) + "] ";
    line.append(msg);
    line.push_back('\n');
    std::cout << line;
    if (stream.is_open())
    {
        stream << line;
        stream.flush();
    }
}

} // namespace studio
