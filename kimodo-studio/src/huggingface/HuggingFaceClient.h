#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace studio {

// Minimal Hugging Face HTTP client (plan section 14).
// Windows: WinHTTP, no third-party deps. HTTPS only.
// Token is passed per-call and never stored or logged here.
class HuggingFaceClient {
public:
    struct HttpResult {
        long status = 0;
        std::string body;
    };

    // GET https://huggingface.co/<path> with optional Bearer token.
    static HttpResult ApiGet(const std::string& path, const std::string& token, std::string& error);

    // Authenticated account name, or empty on failure (error set).
    static std::string Whoami(const std::string& token, std::string& error);

    // Download URL -> tmp file with resume + retry.
    // progress(downloaded, total) runs on caller thread; return false to abort.
    // Returns true on complete download. Partial file kept for resume.
    using ProgressFn = std::function<bool(uint64_t done, uint64_t total)>;
    static bool download(const std::string& url, const std::string& tmp_path, const std::string& token,
                         ProgressFn progress, std::string& error, int retries = 3);

    static std::string ResolveUrl(const std::string& repo, const std::string& remote_path);
};

} // namespace studio
