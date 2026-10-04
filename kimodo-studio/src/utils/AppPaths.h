#pragma once

#include <filesystem>
#include <string_view>

namespace studio
{

class AppPaths
{
public:
    // Core directory locations
    [[nodiscard]] static std::filesystem::path AppDataDir();
    [[nodiscard]] static std::filesystem::path DefaultModelsDir();
    [[nodiscard]] static std::filesystem::path DefaultCharactersDir();
    [[nodiscard]] static std::filesystem::path DefaultAnimationsDir();
    [[nodiscard]] static std::filesystem::path DefaultExportDir();
    [[nodiscard]] static std::filesystem::path config_dir();
    [[nodiscard]] static std::filesystem::path GetSettingsFile();
    [[nodiscard]] static std::filesystem::path CharacterRegistryFile();

    // Resource location resolvers (finds assets in exe dir, working dir, or dev source dir)
    [[nodiscard]] static std::filesystem::path ResolveAsset(std::string_view relative_path);
    [[nodiscard]] static std::filesystem::path ResolveFont(std::string_view font_filename);
    [[nodiscard]] static std::filesystem::path ResolveConfig(std::string_view config_filename);
    [[nodiscard]] static std::filesystem::path ResolveModel(std::string_view model_filename);
    [[nodiscard]] static std::filesystem::path ResolveTextBundle(std::string_view bundle_name);

    // Ensure all critical user directories exist
    static void EnsureDirectories();
};

} // namespace studio
