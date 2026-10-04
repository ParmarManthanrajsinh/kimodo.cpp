#include "huggingface/HuggingFaceClient.h"

#include "utils/Logger.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <thread>
#include <vector>
#if defined(_WIN32)
#include <io.h>
#endif

namespace studio
{
namespace
{

#if defined(_WIN32)
std::wstring Widen(const std::string& s)
{
    if (s.empty())
    {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    w.resize(static_cast<size_t>(n - 1));
    return w;
}

std::string Narrow(const std::wstring& w)
{
    if (w.empty())
    {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    s.resize(static_cast<size_t>(n - 1));
    return s;
}

struct WinHttpHandle
{
    HINTERNET h = nullptr;
    ~WinHttpHandle()
    {
        if (h)
        {
            WinHttpCloseHandle(h);
        }
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET v) : h(v) {}
};

// Crack absolute URL into host + path. Returns false on parse failure.
bool crack_url(const std::string& url, std::wstring& host, std::wstring& path)
{
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    const std::wstring wurl = Widen(url);
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
    {
        return false;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTPS)
    {
        return false; // HTTPS only
    }
    host.assign(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring p(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength > 0)
    {
        p.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    path = p.empty() ? L"/" : p;
    return true;
}

bool query_status(HINTERNET req, long& status)
{
    DWORD code = 0;
    DWORD len = sizeof(code);
    if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &code, &len, WINHTTP_NO_HEADER_INDEX))
    {
        return false;
    }
    status = static_cast<long>(code);
    return true;
}

bool query_header(HINTERNET req, DWORD id, std::string& out)
{
    DWORD len = 0;
    WinHttpQueryHeaders(req, id, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &len, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || len == 0)
    {
        return false;
    }
    std::wstring w(len / sizeof(wchar_t), 0);
    if (!WinHttpQueryHeaders(req, id, WINHTTP_HEADER_NAME_BY_INDEX, w.data(), &len, WINHTTP_NO_HEADER_INDEX))
    {
        return false;
    }
    w.resize(wcslen(w.c_str()));
    out = Narrow(w);
    return true;
}

// Single GET attempt with manual redirect loop. On 200/206 fills bodyFile
// (if non-null) or body string. Returns HTTP status or 0 on transport error.
// A transfer that ends before Content-Length bytes arrive is reported as a
// transport error ("incomplete download"), never as success: silent truncation
// must retry, not hash-fail later as a misleading checksum mismatch.
long get_once(const std::wstring& start_host, const std::wstring& start_path, const std::string& token,
              std::string& body, FILE* body_file, uint64_t resume_offset, uint64_t& content_total,
              HuggingFaceClient::ProgressFn& progress, std::string& error, const std::string& label = {},
              uint64_t range_end = 0)
{
    std::wstring host = start_host;
    std::wstring path = start_path;
    for (int hop = 0; hop < 6; ++hop)
    {
        WinHttpHandle session(WinHttpOpen(L"KimodoStudio/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.h)
        {
            error = "WinHttpOpen failed";
            return 0;
        }
        WinHttpSetTimeouts(session.h, 15000, 15000, 30000, 60000);
        WinHttpHandle conn(WinHttpConnect(session.h, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!conn.h)
        {
            error = "WinHttpConnect failed";
            return 0;
        }
        WinHttpHandle req(WinHttpOpenRequest(conn.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        if (!req.h)
        {
            error = "WinHttpOpenRequest failed";
            return 0;
        }
        DWORD no_redirect = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(req.h, WINHTTP_OPTION_DISABLE_FEATURE, &no_redirect, sizeof(no_redirect));

        const bool same_host = (host == L"huggingface.co");
        if (!token.empty() && same_host)
        {
            const std::wstring auth = L"Authorization: Bearer " + Widen(token);
            WinHttpAddRequestHeaders(req.h, auth.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
        }
        if ((resume_offset > 0 || range_end > 0) && body_file)
        {
            char range[96];
            if (range_end > resume_offset)
            {
                std::snprintf(range, sizeof(range), "Range: bytes=%llu-%llu",
                              static_cast<unsigned long long>(resume_offset),
                              static_cast<unsigned long long>(range_end));
            }
            else
            {
                std::snprintf(range, sizeof(range), "Range: bytes=%llu-",
                              static_cast<unsigned long long>(resume_offset));
            }
            WinHttpAddRequestHeaders(req.h, Widen(range).c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
        }
        if (!WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        {
            error = "WinHttpSendRequest failed";
            return 0;
        }
        if (!WinHttpReceiveResponse(req.h, nullptr))
        {
            error = "WinHttpReceiveResponse failed";
            return 0;
        }
        long status = 0;
        if (!query_status(req.h, status))
        {
            error = "status query failed";
            return 0;
        }
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
        {
            std::string location;
            if (!query_header(req.h, WINHTTP_QUERY_LOCATION, location))
            {
                error = "redirect without Location";
                return 0;
            }
            if (location.rfind("https://", 0) == 0)
            {
                std::wstring new_host, new_path;
                if (!crack_url(location, new_host, new_path))
                {
                    error = "bad redirect URL";
                    return 0;
                }
                host = new_host;
                path = new_path;
            }
            else if (!location.empty() && location[0] == '/')
            {
                // Relative redirect (Hugging Face resolve-cache): same host.
                path = Widen(location);
            }
            else
            {
                error = "unsupported redirect URL";
                return 0;
            }
            continue;
        }
        if (status != 200 && status != 206)
        {
            error = "HTTP " + std::to_string(status);
            return status;
        }
        if (status == 200 && resume_offset > 0 && body_file)
        {
            // Server ignored Range: restart from scratch.
            return -1;
        }
        std::string length_str;
        if (query_header(req.h, WINHTTP_QUERY_CONTENT_LENGTH, length_str))
        {
            try
            {
                content_total = std::stoull(length_str);
                if (status == 206)
                {
                    content_total += resume_offset;
                }
            }
            catch (...)
            {}
        }
        if (body_file && !label.empty())
        {
            // Proof for resume-hostility diagnosis: logged on every attempt.
            std::string accept_ranges;
            if (query_header(req.h, WINHTTP_QUERY_ACCEPT_RANGES, accept_ranges))
            {
                studio::Logger::GetInstance().Info("HuggingFaceClient: " + label + " HTTP " +
                                                   std::to_string(status) +
                                                   " Accept-Ranges: " + accept_ranges);
            }
            else
            {
                studio::Logger::GetInstance().Info("HuggingFaceClient: " + label + " HTTP " +
                                                   std::to_string(status) + " Accept-Ranges: absent");
            }
        }
        std::vector<char> buf(1 << 20);
        uint64_t done = resume_offset;
        uint64_t last_logged = resume_offset;
        auto last_activity = std::chrono::steady_clock::now();
        bool stall_warned = false;
        constexpr uint64_t kLogEveryBytes = 64ULL * 1024ULL * 1024ULL;
        constexpr auto kStallWarnAfter = std::chrono::seconds(30);
        for (;;)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(req.h, &available))
            {
                error = "data-available query failed";
                return 0;
            }
            if (available == 0)
            {
                break; // End of response body reached
            }
            while (available > 0)
            {
                DWORD want = std::min<DWORD>(available, static_cast<DWORD>(buf.size()));
                DWORD got = 0;
                if (!WinHttpReadData(req.h, buf.data(), want, &got) || got == 0)
                {
                    error = "body read failed";
                    return 0;
                }
                if (body_file)
                {
                    if (std::fwrite(buf.data(), 1, got, body_file) != got)
                    {
                        error = "file write failed";
                        return 0;
                    }
                }
                else
                {
                    body.append(buf.data(), got);
                }
                done += got;
                available -= got;
                last_activity = std::chrono::steady_clock::now();
                stall_warned = false;
                if (body_file && !label.empty() && done - last_logged >= kLogEveryBytes)
                {
                    last_logged = done;
                    studio::Logger::GetInstance().Info("HuggingFaceClient: " + label + " downloaded " +
                                                       std::to_string(done) + " bytes");
                }
                if (progress && body_file && !progress(done, content_total))
                {
                    error = "cancelled";
                    return -2;
                }
            }
        }
        if (body_file && content_total > 0 &&
            !HuggingFaceClient::DownloadLengthOk(done, content_total))
        {
            error = "incomplete download (" + std::to_string(done) + " of " +
                    std::to_string(content_total) + " bytes)";
            return 0;
        }
        return status;
    }
    error = "too many redirects";
    return 0;
}
long post_once(const std::wstring& host, const std::wstring& path, const std::string& body_data,
               const std::string& content_type, std::string& out_body, std::string& error)
{
    WinHttpHandle session(WinHttpOpen(L"KimodoStudio/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.h)
    {
        error = "WinHttpOpen failed";
        return 0;
    }
    WinHttpSetTimeouts(session.h, 15000, 15000, 20000, 20000);
    WinHttpHandle conn(WinHttpConnect(session.h, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!conn.h)
    {
        error = "WinHttpConnect failed";
        return 0;
    }
    WinHttpHandle req(WinHttpOpenRequest(conn.h, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!req.h)
    {
        error = "WinHttpOpenRequest failed";
        return 0;
    }

    std::wstring headers;
    if (!content_type.empty())
    {
        headers = L"Content-Type: " + Widen(content_type) + L"\r\n";
    }
    headers += L"Accept: application/json\r\n";

    WinHttpAddRequestHeaders(req.h, headers.c_str(), static_cast<DWORD>(-1),
                             WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    LPVOID post_data = body_data.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body_data.data();
    DWORD post_len = static_cast<DWORD>(body_data.size());

    if (!WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, post_data, post_len, post_len, 0))
    {
        error = "WinHttpSendRequest failed";
        return 0;
    }
    if (!WinHttpReceiveResponse(req.h, nullptr))
    {
        error = "WinHttpReceiveResponse failed";
        return 0;
    }
    long status = 0;
    if (!query_status(req.h, status))
    {
        error = "status query failed";
        return 0;
    }

    out_body.clear();
    std::vector<char> buf(4096);
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(req.h, &available) || available == 0)
        {
            break;
        }
        while (available > 0)
        {
            DWORD want = std::min<DWORD>(available, static_cast<DWORD>(buf.size()));
            DWORD got = 0;
            if (!WinHttpReadData(req.h, buf.data(), want, &got) || got == 0)
            {
                break;
            }
            out_body.append(buf.data(), got);
            available -= got;
        }
    }

    return status;
}
#endif

std::string ExtractJsonString(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
    {
        return {};
    }
    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos)
    {
        return {};
    }
    size_t q1 = json.find('"', colon);
    if (q1 == std::string::npos)
    {
        return {};
    }
    size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos)
    {
        return {};
    }
    return json.substr(q1 + 1, q2 - q1 - 1);
}

int ExtractJsonInt(const std::string& json, const std::string& key, int default_val)
{
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
    {
        return default_val;
    }
    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos)
    {
        return default_val;
    }
    size_t start = json.find_first_of("0123456789", colon);
    if (start == std::string::npos)
    {
        return default_val;
    }
    size_t end = json.find_first_not_of("0123456789", start);
    try
    {
        return std::stoi(json.substr(start, end == std::string::npos ? std::string::npos : end - start));
    }
    catch (...)
    {
        return default_val;
    }
}

} // namespace

HuggingFaceClient::HttpResult HuggingFaceClient::ApiGet(const std::string& path, const std::string& token,
                                                        std::string& error)
{
    HttpResult out;
#if defined(_WIN32)
    ProgressFn none;
    uint64_t total = 0;
    std::string safe_path = path;
    if (!safe_path.empty() && safe_path[0] != '/')
    {
        safe_path = "/" + safe_path;
    }
    const long status = get_once(L"huggingface.co", Widen(safe_path), token, out.body, nullptr, 0, total, none, error);
    if (status > 0)
    {
        out.status = status;
        if (status != 200)
        {
            error = "HTTP " + std::to_string(status);
        }
    }
#else
    (void)path;
    (void)token;
    error = "HF client is Windows-only in this build";
#endif
    return out;
}

HuggingFaceClient::HttpResult HuggingFaceClient::ApiPost(const std::string& path, const std::string& body_data,
                                                         const std::string& content_type, std::string& error)
{
    HttpResult out;
#if defined(_WIN32)
    std::wstring host = L"huggingface.co";
    std::string safe_path = path;
    if (!safe_path.empty() && safe_path[0] != '/')
    {
        safe_path = "/" + safe_path;
    }
    const long status = post_once(host, Widen(safe_path), body_data, content_type, out.body, error);
    if (status > 0)
    {
        out.status = status;
        if (status != 200)
        {
            error = "HTTP " + std::to_string(status);
        }
    }
#else
    (void)path;
    (void)body_data;
    (void)content_type;
    error = "HF client is Windows-only in this build";
#endif
    return out;
}

bool HuggingFaceClient::RequestDeviceCode(DeviceAuthInit& init_out, std::string& error)
{
    const std::string body = "client_id=26be6b09-91c5-47da-9861-d2d2bb7a7e36&scope=openid%20profile%20read-repos";
    HttpResult res = ApiPost("/oauth/device", body, "application/x-www-form-urlencoded", error);
    if (res.status != 200)
    {
        if (error.empty())
        {
            error = "Failed to request device authorization code (HTTP " + std::to_string(res.status) + ")";
        }
        return false;
    }

    init_out.device_code = ExtractJsonString(res.body, "device_code");
    init_out.user_code = ExtractJsonString(res.body, "user_code");
    init_out.verification_uri = ExtractJsonString(res.body, "verification_uri");
    init_out.verification_uri_complete = ExtractJsonString(res.body, "verification_uri_complete");
    init_out.expires_in = ExtractJsonInt(res.body, "expires_in", 900);
    init_out.interval = ExtractJsonInt(res.body, "interval", 5);

    if (init_out.device_code.empty() || init_out.user_code.empty())
    {
        error = "Invalid response from authorization server";
        return false;
    }

    if (init_out.verification_uri_complete.empty() && !init_out.verification_uri.empty())
    {
        std::string base_uri = init_out.verification_uri;
        const std::string hf_short = "https://hf.co";
        if (base_uri.rfind(hf_short, 0) == 0)
        {
            base_uri = "https://huggingface.co" + base_uri.substr(hf_short.size());
        }
        init_out.verification_uri_complete = base_uri + "?user_code=" + init_out.user_code;
    }
    return true;
}

HuggingFaceClient::DeviceAuthPollResult HuggingFaceClient::PollDeviceToken(
    const std::string& device_code, std::string& token_out, std::string& error)
{
    if (device_code.empty())
    {
        error = "Empty device code";
        return DeviceAuthPollResult::Error;
    }

    const std::string body = "client_id=26be6b09-91c5-47da-9861-d2d2bb7a7e36&grant_type=urn:ietf:params:oauth:grant-type:device_code&device_code=" + device_code;
    HttpResult res = ApiPost("/oauth/token", body, "application/x-www-form-urlencoded", error);

    if (res.status == 200)
    {
        token_out = ExtractJsonString(res.body, "access_token");
        if (token_out.empty())
        {
            error = "No access token in response";
            return DeviceAuthPollResult::Error;
        }
        return DeviceAuthPollResult::Success;
    }

    std::string err_code = ExtractJsonString(res.body, "error");
    if (err_code == "authorization_pending")
    {
        return DeviceAuthPollResult::Pending;
    }
    if (err_code == "slow_down")
    {
        return DeviceAuthPollResult::SlowDown;
    }
    if (err_code == "expired_token")
    {
        error = "Authorization session expired";
        return DeviceAuthPollResult::Expired;
    }
    if (err_code == "access_denied")
    {
        error = "Authorization denied by user";
        return DeviceAuthPollResult::Denied;
    }

    error = "Authorization error: " + (err_code.empty() ? ("HTTP " + std::to_string(res.status)) : err_code);
    return DeviceAuthPollResult::Error;
}

bool HuggingFaceClient::CheckModelAccess(const std::string& repo_id, const std::string& token, std::string& error)
{
    if (repo_id.empty())
    {
        return true;
    }
    std::string path = "/api/models/" + repo_id;
    HttpResult r = ApiGet(path, token, error);
    if (r.status == 200)
    {
        return true;
    }
    if (r.status == 401 || r.status == 403)
    {
        error = "Access not granted for gated model: " + repo_id;
        return false;
    }
    if (r.status == 404)
    {
        error = "Model repository not found: " + repo_id;
        return false;
    }
    if (error.empty())
    {
        error = "Failed to query repository access: HTTP " + std::to_string(r.status);
    }
    return false;
}

void HuggingFaceClient::OpenBrowser(const std::string& url)
{
    if (url.empty()) return;
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

std::string HuggingFaceClient::Whoami(const std::string& token, std::string& error)
{
    if (token.empty())
    {
        error = "no token";
        return {};
    }
    HttpResult r = ApiGet("/api/whoami-v2", token, error);
    if (r.status != 200)
    {
        if (r.status == 401)
        {
            error = "invalid token (401)";
        }
        return {};
    }
    // Minimal parse: {"name":"..."}.
    const size_t p = r.body.find("\"name\"");
    if (p == std::string::npos)
    {
        error = "unexpected whoami response";
        return {};
    }
    const size_t c = r.body.find(':', p);
    const size_t q1 = r.body.find('"', c);
    const size_t q2 = r.body.find('"', q1 + 1);
    if (c == std::string::npos || q1 == std::string::npos || q2 == std::string::npos)
    {
        error = "unexpected whoami response";
        return {};
    }
    return r.body.substr(q1 + 1, q2 - q1 - 1);
}

bool HuggingFaceClient::download(const std::string& url, const std::string& tmp_path, const std::string& token,
                                 ProgressFn progress, std::string& error, int retries,
                                 uint64_t expected_total)
{
#if defined(_WIN32)
    std::wstring host, path;
    if (!crack_url(url, host, path))
    {
        error = "bad URL (HTTPS only)";
        return false;
    }
    std::string label = tmp_path;
    {
        const size_t slash = label.find_last_of("/\\");
        if (slash != std::string::npos)
        {
            label = label.substr(slash + 1);
        }
    }
    std::vector<std::string> history;
    for (int attempt = 0; attempt <= retries; ++attempt)
    {
        uint64_t offset = 0;
        {
            FILE* probe = nullptr;
            if (fopen_s(&probe, tmp_path.c_str(), "rb") == 0 && probe)
            {
                _fseeki64(probe, 0, SEEK_END);
                offset = static_cast<uint64_t>(_ftelli64(probe));
                fclose(probe);
            }
        }
        if (!ResumeOffsetOk(offset, expected_total))
        {
            studio::Logger::GetInstance().Warning(
                "HuggingFaceClient: discarding oversized .part for " + label + " (" +
                std::to_string(offset) + " bytes, expected " + std::to_string(expected_total) + ")");
            std::remove(tmp_path.c_str());
            offset = 0;
        }
        FILE* f = nullptr;
        const bool append = offset > 0;
        if (fopen_s(&f, tmp_path.c_str(), append ? "ab" : "wb") != 0 || !f)
        {
            error = "cannot open temp file";
            return false;
        }
        std::string body; // unused for downloads
        uint64_t total = 0;
        long status = get_once(host, path, token, body, f, offset, total, progress, error, label);
        fclose(f);
        if (status == 200 || status == 206)
        {
            studio::Logger::GetInstance().Info("HuggingFaceClient: " + label + " HTTP " +
                                               std::to_string(status) +
                                               (status == 206 ? " (resumed)" : " (full)") +
                                               " attempt " + std::to_string(attempt + 1));
        }
        {
            std::string note = "attempt " + std::to_string(attempt + 1) + ": ";
            note += (status > 0) ? ("HTTP " + std::to_string(status)) : error;
            history.push_back(note);
        }
        if (status == -1)
        {
            // Range ignored: restart without resume.
            std::remove(tmp_path.c_str());
            status = 0;
            error = "server ignored resume; retrying";
        }
        if (status == 200 || status == 206)
        {
            return true;
        }
        if (status == -2 || error == "cancelled")
        {
            error = "cancelled";
            return false;
        }
        if (attempt < retries)
        {
            const int shift = attempt < 3 ? attempt : 3;
            std::this_thread::sleep_for(std::chrono::seconds(1 << shift));
            continue;
        }
        // Terminal failure: attach the per-attempt history so the Setup error
        // dialog (and logs) show the pattern, not just the last symptom.
        error += " [attempts: ";
        for (size_t i = 0; i < history.size(); ++i)
        {
            if (i > 0)
            {
                error += "; ";
            }
            error += history[i];
        }
        error += "]";
        return false;
    }
    error = "download failed";
    return false;
#else
    (void)url;
    (void)tmp_path;
    (void)token;
    (void)progress;
    (void)retries;
    error = "HF client is Windows-only in this build";
    return false;
#endif
}

std::string HuggingFaceClient::ResolveUrl(const std::string& repo, const std::string& remote_path)
{
    return "https://huggingface.co/" + repo + "/resolve/main/" + remote_path;
}

bool HuggingFaceClient::DownloadLengthOk(uint64_t received_bytes, uint64_t expected_total)
{
    if (expected_total == 0)
    {
        return true; // Server supplied no length (e.g. chunked encoding): cannot judge.
    }
    return received_bytes == expected_total;
}

bool HuggingFaceClient::ResumeOffsetOk(uint64_t resume_offset, uint64_t expected_total)
{
    if (expected_total == 0 || resume_offset == 0)
    {
        return true; // Nothing to judge against.
    }
    return resume_offset <= expected_total;
}

uint64_t HuggingFaceClient::ChunkEnd(uint64_t start, uint64_t chunk_bytes, uint64_t expected_total)
{
    if (chunk_bytes == 0 || expected_total == 0 || start >= expected_total)
    {
        return expected_total == 0 ? start : expected_total - 1;
    }
    const uint64_t end = start + chunk_bytes - 1;
    return (end >= expected_total) ? (expected_total - 1) : end;
}

bool HuggingFaceClient::download_chunked(const std::string& url, const std::string& tmp_path,
                                         const std::string& token, ProgressFn progress, std::string& error,
                                         uint64_t expected_total, uint64_t chunk_bytes, int retries_per_chunk)
{
#if defined(_WIN32)
    if (expected_total == 0)
    {
        error = "chunked download requires a known total size";
        return false;
    }
    if (chunk_bytes == 0)
    {
        error = "chunked download requires a nonzero chunk size";
        return false;
    }
    std::wstring host, path;
    if (!crack_url(url, host, path))
    {
        error = "bad URL (HTTPS only)";
        return false;
    }
    std::string label = tmp_path;
    if (const size_t slash = label.find_last_of("/\\"); slash != std::string::npos)
    {
        label = label.substr(slash + 1);
    }
    studio::Logger::GetInstance().Info("HuggingFaceClient: " + label + " chunked download (" +
                                       std::to_string(expected_total) + " bytes)");

    uint64_t start = 0;
    {
        // Resume inside the chunked flow: round down to a chunk boundary so
        // every chunk is re-fetched whole (a short tail is simply re-done).
        FILE* probe = nullptr;
        if (fopen_s(&probe, tmp_path.c_str(), "rb") == 0 && probe)
        {
            _fseeki64(probe, 0, SEEK_END);
            uint64_t have = static_cast<uint64_t>(_ftelli64(probe));
            fclose(probe);
            if (have > expected_total)
            {
                studio::Logger::GetInstance().Warning("HuggingFaceClient: discarding oversized .part for " +
                                                      label);
                std::remove(tmp_path.c_str());
            }
            else if (have > 0)
            {
                start = (have / chunk_bytes) * chunk_bytes;
                if (start != have)
                {
                    // Truncate the ragged tail; the chunk restarts cleanly.
                    FILE* trunc = nullptr;
                    if (fopen_s(&trunc, tmp_path.c_str(), "r+b") == 0 && trunc)
                    {
                        _chsize_s(_fileno(trunc), static_cast<__int64>(start));
                        fclose(trunc);
                    }
                }
                studio::Logger::GetInstance().Info("HuggingFaceClient: " + label + " resuming chunks at byte " +
                                                   std::to_string(start));
            }
        }
    }

    const uint64_t total_chunks =
        (expected_total + chunk_bytes - 1) / chunk_bytes;
    uint64_t chunk_index = (chunk_bytes == 0) ? 0 : (start / chunk_bytes);
    while (start < expected_total)
    {
        const uint64_t end = ChunkEnd(start, chunk_bytes, expected_total);
        bool chunk_ok = false;
        std::string chunk_err;
        for (int attempt = 0; attempt <= retries_per_chunk && !chunk_ok; ++attempt)
        {
            FILE* f = nullptr;
            if (fopen_s(&f, tmp_path.c_str(), "ab") != 0 || !f)
            {
                error = "cannot open temp file";
                return false;
            }
            // Guard: the file must be exactly `start` bytes or this chunk
            // would be appended at the wrong offset.
            _fseeki64(f, 0, SEEK_END);
            const uint64_t pos = static_cast<uint64_t>(_ftelli64(f));
            std::string body;
            uint64_t total = 0;
            if (pos != start)
            {
                fclose(f);
                chunk_err = "chunk offset skew (file has " + std::to_string(pos) + " bytes, chunk starts at " +
                            std::to_string(start) + ")";
                break;
            }
            long status =
                get_once(host, path, token, body, f, start, total, progress, chunk_err, label, end);
            fclose(f);
            if (status == -1)
            {
                chunk_err = "server does not honor byte ranges; chunked fallback unavailable";
                break; // Retrying would just repeat the full-body response.
            }
            if (status == -2 || chunk_err == "cancelled")
            {
                error = "cancelled";
                return false;
            }
            if (status != 206)
            {
                chunk_err = "chunk " + std::to_string(chunk_index + 1) + "/" + std::to_string(total_chunks) +
                            " unexpected HTTP " + std::to_string(status) +
                            (chunk_err.empty() ? "" : " (" + chunk_err + ")");
                if (attempt < retries_per_chunk)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
                }
                continue;
            }
            // 206 with an explicit range: the chunk must be exactly end-start+1.
            std::error_code ec;
            const auto have = std::filesystem::file_size(tmp_path, ec);
            if (!ec && have == end + 1)
            {
                chunk_ok = true;
            }
            else
            {
                chunk_err = "chunk " + std::to_string(chunk_index + 1) + "/" + std::to_string(total_chunks) +
                            " short body";
                // Truncate any ragged bytes so the retry appends cleanly.
                FILE* trunc = nullptr;
                if (fopen_s(&trunc, tmp_path.c_str(), "r+b") == 0 && trunc)
                {
                    _chsize_s(_fileno(trunc), static_cast<__int64>(start));
                    fclose(trunc);
                }
                if (attempt < retries_per_chunk)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
                }
            }
        }
        if (!chunk_ok)
        {
            error = chunk_err.empty() ? "chunked download failed" : chunk_err;
            return false;
        }
        studio::Logger::GetInstance().Info("HuggingFaceClient: " + label + " chunk " +
                                           std::to_string(chunk_index + 1) + "/" +
                                           std::to_string(total_chunks) + " complete");
        start = end + 1;
        ++chunk_index;
    }
    return true;
#else
    (void)url;
    (void)tmp_path;
    (void)token;
    (void)progress;
    (void)retries_per_chunk;
    (void)chunk_bytes;
    (void)expected_total;
    error = "HF client is Windows-only in this build";
    return false;
#endif
}

} // namespace studio
