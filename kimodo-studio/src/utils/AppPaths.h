#pragma once

#include <filesystem>
#include <string>

namespace studio {

class AppPaths {
public:
    // Core directory locations
    static std::filesystem::path AppDataDir();
    static std::filesystem::path DefaultModelsDir();
    static std::filesystem::path DefaultCharactersDir();
    static std::filesystem::path DefaultAnimationsDir();
    static std::filesystem::path DefaultExportDir();
    static std::filesystem::path config_dir();
    static std::filesystem::path GetSettingsFile();
    static std::filesystem::path CharacterRegistryFile();

    // Resource location resolvers (finds assets in exe dir, working dir, or dev source dir)
    static std::filesystem::path ResolveAsset(const std::string& relative_path);
    static std::filesystem::path ResolveFont(const std::string& font_filename);
    static std::filesystem::path ResolveConfig(const std::string& config_filename);
    static std::filesystem::path ResolveModel(const std::string& model_filename);
    static std::filesystem::path ResolveTextBundle(const std::string& bundle_name);

    // Ensure all critical user directories exist
    static void EnsureDirectories();
};

} // namespace studio
