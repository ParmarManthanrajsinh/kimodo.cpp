#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace studio {

class FAppPaths {
public:
    // Core directory locations
    static std::filesystem::path appDataDir();
    static std::filesystem::path defaultModelsDir();
    static std::filesystem::path defaultCharactersDir();
    static std::filesystem::path defaultAnimationsDir();
    static std::filesystem::path defaultExportDir();
    static std::filesystem::path configDir();
    static std::filesystem::path GetSettingsFile();
    static std::filesystem::path characterRegistryFile();

    // Resource location resolvers (finds assets in exe dir, working dir, or dev source dir)
    static std::filesystem::path resolveAsset(const std::string& relativePath);
    static std::filesystem::path resolveFont(const std::string& fontFilename);
    static std::filesystem::path resolveConfig(const std::string& configFilename);
    static std::filesystem::path resolveModel(const std::string& modelFilename);
    static std::filesystem::path resolveTextBundle(const std::string& bundleName);

    // Ensure all critical user directories exist
    static void ensureDirectories();
};

} // namespace studio
