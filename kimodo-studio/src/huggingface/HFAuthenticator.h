#pragma once

#include <string>

namespace studio
{

// Hugging Face token handling (plan section 14):
// - token lives in OS secure storage (Windows Credential Manager)
// - never in source, never in logs, never in plain files
class HFAuthenticator
{
public:
    static bool SaveToken(const std::string& token, std::string& error);
    static bool LoadToken(std::string& token);
    static bool ClearToken();

    // Validate against /api/whoami-v2. Returns account name or "".
    static std::string Validate(const std::string& token, std::string& error);

    static const char* CredTarget();
};

} // namespace studio
