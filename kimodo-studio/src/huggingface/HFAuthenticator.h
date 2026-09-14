#pragma once

#include <string>

namespace studio {

// Hugging Face token handling (plan section 14):
// - token lives in OS secure storage (Windows Credential Manager)
// - never in source, never in logs, never in plain files
class HFAuthenticator {
public:
    static bool saveToken(const std::string& token, std::string& error);
    static bool loadToken(std::string& token);
    static bool clearToken();

    // Validate against /api/whoami-v2. Returns account name or "".
    static std::string validate(const std::string& token, std::string& error);

    static const char* credTarget();
};

} // namespace studio
