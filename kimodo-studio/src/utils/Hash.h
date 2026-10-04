#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace studio
{

// SHA-256 over a file, chunked, with progress. Windows: BCrypt.
// Returns hex digest or empty on error.
class FileHash
{
public:
    using ProgressFn = std::function<void(uint64_t done, uint64_t total)>;
    static std::string Sha256(const std::string& path, std::string& error, ProgressFn progress = {});
};

} // namespace studio
