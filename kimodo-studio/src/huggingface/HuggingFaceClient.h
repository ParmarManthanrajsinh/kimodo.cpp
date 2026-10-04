#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace studio
{

// Minimal Hugging Face HTTP client (plan section 14).
// Windows: WinHTTP, no third-party deps. HTTPS only.
// Token is passed per-call and never stored or logged here.
class HuggingFaceClient
{
public:
    struct HttpResult
    {
        long status = 0;
        std::string body;
    };

    struct DeviceAuthInit
    {
        std::string device_code;
        std::string user_code;
        std::string verification_uri;
        std::string verification_uri_complete;
        int expires_in = 900;
        int interval = 5;
    };

    enum class DeviceAuthPollResult
    {
        Pending,
        Success,
        SlowDown,
        Expired,
        Denied,
        Error
    };

    // GET https://huggingface.co/<path> with optional Bearer token.
    static HttpResult ApiGet(const std::string& path, const std::string& token, std::string& error);

    // POST https://huggingface.co/<path> with body and optional content-type.
    static HttpResult ApiPost(const std::string& path, const std::string& body_data,
                              const std::string& content_type, std::string& error);

    // Request OAuth Device Code for native desktop authentication (RFC 8628).
    static bool RequestDeviceCode(DeviceAuthInit& init_out, std::string& error);

    // Poll for OAuth token using device_code.
    static DeviceAuthPollResult PollDeviceToken(const std::string& device_code, std::string& token_out,
                                                std::string& error);

    // Check if token has read access to a specific repository.
    static bool CheckModelAccess(const std::string& repo_id, const std::string& token, std::string& error);

    // Launch default OS browser to specified URL.
    static void OpenBrowser(const std::string& url);

    // Authenticated account name, or empty on failure (error set).
    static std::string Whoami(const std::string& token, std::string& error);

    // Download URL -> tmp file with resume + retry.
    // progress(downloaded, total) runs on caller thread; return false to abort.
    // Returns true on complete download. Partial file kept for resume.
    // expected_total (0 = unknown) enables resume-offset sanity: an oversized
    // .part is discarded instead of sending a nonsense Range request.
    using ProgressFn = std::function<bool(uint64_t done, uint64_t total)>;
    static bool download(const std::string& url, const std::string& tmp_path, const std::string& token,
                         ProgressFn progress, std::string& error, int retries = 3,
                         uint64_t expected_total = 0);

    static std::string ResolveUrl(const std::string& repo, const std::string& remote_path);

    // Byte-count validation for completed transfers. Pure function (no WinHTTP),
    // safe to unit test: true when the server supplied no length (0) or the
    // received count matches the expected total exactly.
    static bool DownloadLengthOk(uint64_t received_bytes, uint64_t expected_total);
    // Resume sanity: a .part larger than the registry size can never be a
    // valid prefix (wrong file / corrupt). Pure function, safe to unit test.
    static bool ResumeOffsetOk(uint64_t resume_offset, uint64_t expected_total);
    // Chunked fallback for resume-hostile networks: download [0, expected)
    // as explicit bounded byte ranges appended in order, each length-checked.
    // Returns true only when the final file is exactly expected_total bytes.
    // expected_total == 0 is rejected (unbounded targets cannot be chunked).
    static bool download_chunked(const std::string& url, const std::string& tmp_path,
                                 const std::string& token, ProgressFn progress, std::string& error,
                                 uint64_t expected_total, uint64_t chunk_bytes = 64ULL * 1024ULL * 1024ULL,
                                 int retries_per_chunk = 2);
    // Last-byte index (inclusive) of the chunk starting at `start`.
    // Pure function, safe to unit test.
    static uint64_t ChunkEnd(uint64_t start, uint64_t chunk_bytes, uint64_t expected_total);
};

} // namespace studio
