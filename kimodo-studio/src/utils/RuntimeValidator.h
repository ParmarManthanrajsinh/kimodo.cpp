#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace studio
{

struct RuntimeValidation
{
    bool runtime_dlls_valid = false;
    bool vulkan_available = false;
    bool gpu_accelerated = false;
    bool memory_ready = false;
    std::string gpu_name = "CPU Software";
    std::string vulkan_driver_version;
    std::string runtime_details;
    std::string vulkan_status_message;
    std::string memory_details;
    std::string backend_name = "CPU / OpenMP";
    std::string vram_info = "System RAM";
    std::string runtime_version = "GGML v0.9.2";
    std::vector<std::string> missing_dlls;
};

class RuntimeValidator
{
public:
    static RuntimeValidation Validate();
    static bool CheckLibraries(const std::filesystem::path& root_dir, std::vector<std::string>& missing_out);
};

} // namespace studio
