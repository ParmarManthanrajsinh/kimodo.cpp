#include "utils/Logger.h"
#include "utils/AppPaths.h"

#include <chrono>
#include <format>
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
    return AppPaths::AppDataDir() / "logs" / "kimodo-studio.log";
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
    const auto local = std::chrono::current_zone()->to_local(now);
    return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::floor<std::chrono::seconds>(local));
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
