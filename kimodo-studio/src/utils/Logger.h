#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

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
    [[nodiscard]] static Logger& GetInstance() noexcept;

    void Init(const std::filesystem::path& file);
    void Log(LogLevel level, std::string_view msg);

    void trace(std::string_view msg)
    {
        Log(LogLevel::Trace, msg);
    }
    void Debug(std::string_view msg)
    {
        Log(LogLevel::Debug, msg);
    }
    void Info(std::string_view msg)
    {
        Log(LogLevel::Info, msg);
    }
    void Warning(std::string_view msg)
    {
        Log(LogLevel::Warning, msg);
    }
    void Error(std::string_view msg)
    {
        Log(LogLevel::Error, msg);
    }

    [[nodiscard]] static std::filesystem::path DefaultLogFile();

private:
    Logger() = default;
    [[nodiscard]] static const char* LevelName(LogLevel level) noexcept;

    std::mutex mutex;
    std::ofstream stream;
};

} // namespace studio
