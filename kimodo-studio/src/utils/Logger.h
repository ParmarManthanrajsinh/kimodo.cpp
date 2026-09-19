#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace studio
{

enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warning,
    Error
};

class Logger
{
public:
    static Logger& GetInstance();

    void Init(const std::filesystem::path& file);
    void Log(LogLevel level, const std::string& msg);

    void trace(const std::string& msg)
    {
        Log(LogLevel::Trace, msg);
    }
    void Debug(const std::string& msg)
    {
        Log(LogLevel::Debug, msg);
    }
    void Info(const std::string& msg)
    {
        Log(LogLevel::Info, msg);
    }
    void Warning(const std::string& msg)
    {
        Log(LogLevel::Warning, msg);
    }
    void Error(const std::string& msg)
    {
        Log(LogLevel::Error, msg);
    }

    static std::filesystem::path DefaultLogFile();

private:
    Logger() = default;
    static const char* LevelName(LogLevel level);

    std::mutex mutex;
    std::ofstream stream;
};

} // namespace studio
