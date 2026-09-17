#include "utils/AppPaths.h"

#include <cstdlib>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace studio {
namespace {

std::filesystem::path getExecutableDir() {
#if defined(_WIN32)
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len > 0) {
        return std::filesystem::path(path).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

} // namespace

std::filesystem::path AppPaths::appDataDir() {
#if defined(_WIN32)
    if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path(localAppData) / "KimodoStudio";
    }
    if (const char* appData = std::getenv("APPDATA")) {
        return std::filesystem::path(appData) / "KimodoStudio";
    }
    return std::filesystem::path("user_data");
#else
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".kimodo-studio";
    }
    return std::filesystem::path("user_data");
#endif
}

std::filesystem::path AppPaths::defaultModelsDir() {
    return appDataDir() / "models";
}

std::filesystem::path AppPaths::defaultCharactersDir() {
    return appDataDir() / "characters";
}

std::filesystem::path AppPaths::defaultAnimationsDir() {
    return appDataDir() / "animations";
}

std::filesystem::path AppPaths::defaultExportDir() {
#if defined(_WIN32)
    if (const char* userProfile = std::getenv("USERPROFILE")) {
        return std::filesystem::path(userProfile) / "Documents" / "KimodoStudio" / "Exports";
    }
#endif
    return appDataDir() / "exports";
}

std::filesystem::path AppPaths::configDir() {
    return appDataDir() / "config";
}

std::filesystem::path AppPaths::settingsFile() {
    return appDataDir() / "settings.json";
}

std::filesystem::path AppPaths::characterRegistryFile() {
    return appDataDir() / "characters.json";
}

std::filesystem::path AppPaths::resolveAsset(const std::string& relativePath) {
    const std::filesystem::path exeDir = getExecutableDir();
    std::vector<std::filesystem::path> candidates = {
        exeDir / relativePath,
        std::filesystem::current_path() / relativePath,
#if defined(KIMODO_STUDIO_SOURCE_DIR)
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / relativePath,
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / ".." / relativePath,
#endif
#if defined(KIMODO_ROOT_DIR)
        std::filesystem::path(KIMODO_ROOT_DIR) / relativePath,
#endif
        appDataDir() / relativePath
    };

    // Walk up from exeDir searching
    std::filesystem::path cur = exeDir;
    for (int i = 0; i < 5; ++i) {
        if (!cur.empty()) {
            candidates.push_back(cur / relativePath);
            cur = cur.parent_path();
        }
    }

    std::error_code ec;
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p, ec) && !ec) {
            return p;
        }
    }
    return candidates.front();
}

std::filesystem::path AppPaths::resolveFont(const std::string& fontFilename) {
    return resolveAsset("fonts/" + fontFilename);
}

std::filesystem::path AppPaths::resolveConfig(const std::string& configFilename) {
    // Check user data config first, then bundled config
    std::error_code ec;
    const auto userConfig = configDir() / configFilename;
    if (std::filesystem::is_regular_file(userConfig, ec) && !ec) {
        return userConfig;
    }
    return resolveAsset("config/" + configFilename);
}

std::filesystem::path AppPaths::resolveModel(const std::string& modelFilename) {
    std::error_code ec;
    const auto userModel = defaultModelsDir() / modelFilename;
    if (std::filesystem::is_regular_file(userModel, ec) && !ec) {
        return userModel;
    }
    return resolveAsset("models/" + modelFilename);
}

std::filesystem::path AppPaths::resolveTextBundle(const std::string& bundleName) {
    const std::filesystem::path exeDir = getExecutableDir();
    std::vector<std::filesystem::path> candidates = {
        defaultModelsDir() / bundleName,
        appDataDir() / "generated" / bundleName,
        appDataDir() / bundleName,
        exeDir / bundleName,
        exeDir / "generated" / bundleName,
        exeDir / "models" / bundleName,
        std::filesystem::current_path() / bundleName,
        std::filesystem::current_path() / "generated" / bundleName,
        std::filesystem::current_path() / "models" / bundleName,
#if defined(KIMODO_ROOT_DIR)
        std::filesystem::path(KIMODO_ROOT_DIR) / "generated" / bundleName,
        std::filesystem::path(KIMODO_ROOT_DIR) / "models" / bundleName,
        std::filesystem::path(KIMODO_ROOT_DIR) / bundleName,
#endif
#if defined(KIMODO_STUDIO_SOURCE_DIR)
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "generated" / bundleName,
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "../generated" / bundleName,
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "../models" / bundleName,
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / ".." / bundleName,
#endif
    };

    // Walk up from exeDir searching
    std::filesystem::path cur = exeDir;
    for (int i = 0; i < 5; ++i) {
        if (!cur.empty()) {
            candidates.push_back(cur / "generated" / bundleName);
            candidates.push_back(cur / bundleName);
            candidates.push_back(cur / "models" / bundleName);
            cur = cur.parent_path();
        }
    }

    cur = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        if (!cur.empty()) {
            candidates.push_back(cur / "generated" / bundleName);
            candidates.push_back(cur / bundleName);
            candidates.push_back(cur / "models" / bundleName);
            cur = cur.parent_path();
        }
    }

    std::error_code ec;
    for (const auto& p : candidates) {
        if (std::filesystem::is_directory(p, ec) && !ec) {
            return p;
        }
    }

    return candidates.front();
}

void AppPaths::ensureDirectories() {
    std::error_code ec;
    std::filesystem::create_directories(appDataDir(), ec);
    std::filesystem::create_directories(defaultModelsDir(), ec);
    std::filesystem::create_directories(defaultCharactersDir(), ec);
    std::filesystem::create_directories(defaultAnimationsDir(), ec);
    std::filesystem::create_directories(defaultExportDir(), ec);
    std::filesystem::create_directories(configDir(), ec);
}

} // namespace studio
