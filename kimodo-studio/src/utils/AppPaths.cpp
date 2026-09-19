#include "utils/AppPaths.h"

#include <cstdlib>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace studio {
namespace {

std::filesystem::path get_executable_dir() {
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

std::filesystem::path AppPaths::AppDataDir() {
#if defined(_WIN32)
    if (const char* local_app_data = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path(local_app_data) / "KimodoStudio";
    }
    if (const char* app_data = std::getenv("APPDATA")) {
        return std::filesystem::path(app_data) / "KimodoStudio";
    }
    return std::filesystem::path("user_data");
#else
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".kimodo-studio";
    }
    return std::filesystem::path("user_data");
#endif
}

std::filesystem::path AppPaths::DefaultModelsDir() { return AppDataDir() / "models"; }

std::filesystem::path AppPaths::DefaultCharactersDir() { return AppDataDir() / "characters"; }

std::filesystem::path AppPaths::DefaultAnimationsDir() { return AppDataDir() / "animations"; }

std::filesystem::path AppPaths::DefaultExportDir() {
#if defined(_WIN32)
    if (const char* user_profile = std::getenv("USERPROFILE")) {
        return std::filesystem::path(user_profile) / "Documents" / "KimodoStudio" / "Exports";
    }
#endif
    return AppDataDir() / "exports";
}

std::filesystem::path AppPaths::config_dir() { return AppDataDir() / "config"; }

std::filesystem::path AppPaths::GetSettingsFile() { return AppDataDir() / "settings.json"; }

std::filesystem::path AppPaths::CharacterRegistryFile() { return AppDataDir() / "characters.json"; }

std::filesystem::path AppPaths::ResolveAsset(const std::string& relative_path) {
    const std::filesystem::path exe_dir = get_executable_dir();
    std::vector<std::filesystem::path> candidates = {exe_dir / relative_path,
                                                     exe_dir / "assets" / relative_path,
                                                     std::filesystem::current_path() / relative_path,
                                                     std::filesystem::current_path() / "assets" / relative_path,
#if defined(KIMODO_STUDIO_SOURCE_DIR)
                                                     std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / relative_path,
                                                     std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "assets" /
                                                         relative_path,
                                                     std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / ".." /
                                                         relative_path,
#endif
#if defined(KIMODO_ROOT_DIR)
                                                     std::filesystem::path(KIMODO_ROOT_DIR) / relative_path,
#endif
                                                     AppDataDir() / relative_path,
                                                     AppDataDir() / "assets" / relative_path};

    // Walk up from exeDir searching
    std::filesystem::path cur = exe_dir;
    for (int i = 0; i < 5; ++i) {
        if (!cur.empty()) {
            candidates.push_back(cur / relative_path);
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

std::filesystem::path AppPaths::ResolveFont(const std::string& font_filename) {
    return ResolveAsset("fonts/" + font_filename);
}

std::filesystem::path AppPaths::ResolveConfig(const std::string& config_filename) {
    // Check user data config first, then bundled config
    std::error_code ec;
    const auto user_config = config_dir() / config_filename;
    if (std::filesystem::is_regular_file(user_config, ec) && !ec) {
        return user_config;
    }
    return ResolveAsset("config/" + config_filename);
}

std::filesystem::path AppPaths::ResolveModel(const std::string& model_filename) {
    std::error_code ec;
    const auto user_model = DefaultModelsDir() / model_filename;
    if (std::filesystem::is_regular_file(user_model, ec) && !ec) {
        return user_model;
    }
    return ResolveAsset("models/" + model_filename);
}

std::filesystem::path AppPaths::ResolveTextBundle(const std::string& bundle_name) {
    const std::filesystem::path exe_dir = get_executable_dir();
    std::vector<std::filesystem::path> candidates = {
        DefaultModelsDir() / bundle_name,
        AppDataDir() / "generated" / bundle_name,
        AppDataDir() / bundle_name,
        exe_dir / bundle_name,
        exe_dir / "generated" / bundle_name,
        exe_dir / "models" / bundle_name,
        std::filesystem::current_path() / bundle_name,
        std::filesystem::current_path() / "generated" / bundle_name,
        std::filesystem::current_path() / "models" / bundle_name,
#if defined(KIMODO_ROOT_DIR)
        std::filesystem::path(KIMODO_ROOT_DIR) / "generated" / bundle_name,
        std::filesystem::path(KIMODO_ROOT_DIR) / "models" / bundle_name,
        std::filesystem::path(KIMODO_ROOT_DIR) / bundle_name,
#endif
#if defined(KIMODO_STUDIO_SOURCE_DIR)
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "generated" / bundle_name,
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "../generated" / bundle_name,
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "../models" / bundle_name,
        std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / ".." / bundle_name,
#endif
    };

    // Walk up from exeDir searching
    std::filesystem::path cur = exe_dir;
    for (int i = 0; i < 5; ++i) {
        if (!cur.empty()) {
            candidates.push_back(cur / "generated" / bundle_name);
            candidates.push_back(cur / bundle_name);
            candidates.push_back(cur / "models" / bundle_name);
            cur = cur.parent_path();
        }
    }

    cur = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        if (!cur.empty()) {
            candidates.push_back(cur / "generated" / bundle_name);
            candidates.push_back(cur / bundle_name);
            candidates.push_back(cur / "models" / bundle_name);
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

void AppPaths::EnsureDirectories() {
    std::error_code ec;
    std::filesystem::create_directories(AppDataDir(), ec);
    std::filesystem::create_directories(DefaultModelsDir(), ec);
    std::filesystem::create_directories(DefaultCharactersDir(), ec);
    std::filesystem::create_directories(DefaultAnimationsDir(), ec);
    std::filesystem::create_directories(DefaultExportDir(), ec);
    std::filesystem::create_directories(config_dir(), ec);
}

} // namespace studio
