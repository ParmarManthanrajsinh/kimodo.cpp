#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace studio {

enum class ELogLevel { Trace, Debug, Info, Warning, Error };

class FLogger {
public:
    static FLogger& GetInstance();

    void Init(const std::filesystem::path& file);
    void Log(ELogLevel level, const std::string& msg);

    void trace(const std::string& msg) { Log(ELogLevel::Trace, msg); }
    void debug(const std::string& msg) { Log(ELogLevel::Debug, msg); }
    void info(const std::string& msg) { Log(ELogLevel::Info, msg); }
    void warning(const std::string& msg) { Log(ELogLevel::Warning, msg); }
    void error(const std::string& msg) { Log(ELogLevel::Error, msg); }

    static std::filesystem::path DefaultLogFile();

private:
    FLogger() = default;
    static const char* LevelName(ELogLevel level);

    std::mutex mutex;
    std::ofstream stream;
};

} // namespace studio
