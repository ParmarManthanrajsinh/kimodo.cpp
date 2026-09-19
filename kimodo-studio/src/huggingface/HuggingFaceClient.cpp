#include "huggingface/HuggingFaceClient.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace studio {
namespace {

#if defined(_WIN32)
std::wstring Widen(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    w.resize(static_cast<size_t>(n - 1));
    return w;
}

std::string Narrow(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    s.resize(static_cast<size_t>(n - 1));
    return s;
}

struct WinHttpHandle {
    HINTERNET h = nullptr;
    ~WinHttpHandle() {
        if (h) {
            WinHttpCloseHandle(h);
        }
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET v) : h(v) {}
};

// Crack absolute URL into host + path. Returns false on parse failure.
bool crack_url(const std::string& url, std::wstring& host, std::wstring& path) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    const std::wstring wurl = Widen(url);
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts)) {
        return false;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTPS) {
        return false; // HTTPS only
    }
    host.assign(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring p(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength > 0) {
        p.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    path = p.empty() ? L"/" : p;
    return true;
}

bool query_status(HINTERNET req, long& status) {
    DWORD code = 0;
    DWORD len = sizeof(code);
    if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &code, &len, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    status = static_cast<long>(code);
    return true;
}

bool query_header(HINTERNET req, DWORD id, std::string& out) {
    DWORD len = 0;
    WinHttpQueryHeaders(req, id, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &len, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || len == 0) {
        return false;
    }
    std::wstring w(len / sizeof(wchar_t), 0);
    if (!WinHttpQueryHeaders(req, id, WINHTTP_HEADER_NAME_BY_INDEX, w.data(), &len, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    w.resize(wcslen(w.c_str()));
    out = Narrow(w);
    return true;
}

// Single GET attempt with manual redirect loop. On 200/206 fills bodyFile
// (if non-null) or body string. Returns HTTP status or 0 on transport error.
long get_once(const std::wstring& start_host, const std::wstring& start_path, const std::string& token,
              std::string& body, FILE* body_file, uint64_t resume_offset, uint64_t& content_total,
              HuggingFaceClient::ProgressFn& progress, std::string& error) {
    std::wstring host = start_host;
    std::wstring path = start_path;
    for (int hop = 0; hop < 6; ++hop) {
        WinHttpHandle session(WinHttpOpen(L"KimodoStudio/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.h) {
            error = "WinHttpOpen failed";
            return 0;
        }
        WinHttpSetTimeouts(session.h, 15000, 15000, 30000, 60000);
        WinHttpHandle conn(WinHttpConnect(session.h, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!conn.h) {
            error = "WinHttpConnect failed";
            return 0;
        }
        WinHttpHandle req(WinHttpOpenRequest(conn.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        if (!req.h) {
            error = "WinHttpOpenRequest failed";
            return 0;
        }
        DWORD no_redirect = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(req.h, WINHTTP_OPTION_DISABLE_FEATURE, &no_redirect, sizeof(no_redirect));

        const bool same_host = (host == L"huggingface.co");
        if (!token.empty() && same_host) {
            const std::wstring auth = L"Authorization: Bearer " + Widen(token);
            WinHttpAddRequestHeaders(req.h, auth.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
        }
        if (resume_offset > 0 && body_file) {
            char range[64];
            std::snprintf(range, sizeof(range), "Range: bytes=%llu-", static_cast<unsigned long long>(resume_offset));
            WinHttpAddRequestHeaders(req.h, Widen(range).c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
        }
        if (!WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            error = "WinHttpSendRequest failed";
            return 0;
        }
        if (!WinHttpReceiveResponse(req.h, nullptr)) {
            error = "WinHttpReceiveResponse failed";
            return 0;
        }
        long status = 0;
        if (!query_status(req.h, status)) {
            error = "status query failed";
            return 0;
        }
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            std::string location;
            if (!query_header(req.h, WINHTTP_QUERY_LOCATION, location)) {
                error = "redirect without Location";
                return 0;
            }
            if (location.rfind("https://", 0) == 0) {
                std::wstring new_host, new_path;
                if (!crack_url(location, new_host, new_path)) {
                    error = "bad redirect URL";
                    return 0;
                }
                host = new_host;
                path = new_path;
            } else if (!location.empty() && location[0] == '/') {
                // Relative redirect (Hugging Face resolve-cache): same host.
                path = Widen(location);
            } else {
                error = "unsupported redirect URL";
                return 0;
            }
            continue;
        }
        if (status != 200 && status != 206) {
            error = "HTTP " + std::to_string(status);
            return status;
        }
        if (status == 200 && resume_offset > 0 && body_file) {
            // Server ignored Range: restart from scratch.
            return -1;
        }
        std::string length_str;
        if (query_header(req.h, WINHTTP_QUERY_CONTENT_LENGTH, length_str)) {
            try {
                content_total = std::stoull(length_str);
                if (status == 206) {
                    content_total += resume_offset;
                }
            } catch (...) {
            }
        }
        DWORD chunk = 0;
        std::vector<char> buf(1 << 20);
        uint64_t done = resume_offset;
        do {
            chunk = 0;
            if (!WinHttpQueryDataAvailable(req.h, &chunk)) {
                error = "data-available query failed";
                return 0;
            }
            while (chunk > 0) {
                DWORD want = std::min<DWORD>(chunk, static_cast<DWORD>(buf.size()));
                DWORD got = 0;
                if (!WinHttpReadData(req.h, buf.data(), want, &got) || got == 0) {
                    error = "body read failed";
                    return 0;
                }
                if (body_file) {
                    if (std::fwrite(buf.data(), 1, got, body_file) != got) {
                        error = "file write failed";
                        return 0;
                    }
                } else {
                    body.append(buf.data(), got);
                }
                done += got;
                chunk -= got;
                if (progress && body_file && !progress(done, content_total)) {
                    error = "cancelled";
                    return -2;
                }
            }
        } while (chunk > 0);
        return status;
    }
    error = "too many redirects";
    return 0;
}
#endif

} // namespace

HuggingFaceClient::HttpResult HuggingFaceClient::ApiGet(const std::string& path, const std::string& token,
                                                        std::string& error) {
    HttpResult out;
#if defined(_WIN32)
    ProgressFn none;
    uint64_t total = 0;
    const long status = get_once(L"huggingface.co", Widen(path), token, out.body, nullptr, 0, total, none, error);
    if (status > 0) {
        out.status = status;
        if (status != 200) {
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

std::string HuggingFaceClient::Whoami(const std::string& token, std::string& error) {
    if (token.empty()) {
        error = "no token";
        return {};
    }
    HttpResult r = ApiGet("/api/whoami-v2", token, error);
    if (r.status != 200) {
        if (r.status == 401) {
            error = "invalid token (401)";
        }
        return {};
    }
    // Minimal parse: {"name":"..."}.
    const size_t p = r.body.find("\"name\"");
    if (p == std::string::npos) {
        error = "unexpected whoami response";
        return {};
    }
    const size_t c = r.body.find(':', p);
    const size_t q1 = r.body.find('"', c);
    const size_t q2 = r.body.find('"', q1 + 1);
    if (c == std::string::npos || q1 == std::string::npos || q2 == std::string::npos) {
        error = "unexpected whoami response";
        return {};
    }
    return r.body.substr(q1 + 1, q2 - q1 - 1);
}

bool HuggingFaceClient::download(const std::string& url, const std::string& tmp_path, const std::string& token,
                                 ProgressFn progress, std::string& error, int retries) {
#if defined(_WIN32)
    std::wstring host, path;
    if (!crack_url(url, host, path)) {
        error = "bad URL (HTTPS only)";
        return false;
    }
    for (int attempt = 0; attempt <= retries; ++attempt) {
        uint64_t offset = 0;
        {
            FILE* probe = nullptr;
            if (fopen_s(&probe, tmp_path.c_str(), "rb") == 0 && probe) {
                _fseeki64(probe, 0, SEEK_END);
                offset = static_cast<uint64_t>(_ftelli64(probe));
                fclose(probe);
            }
        }
        FILE* f = nullptr;
        const bool append = offset > 0;
        if (fopen_s(&f, tmp_path.c_str(), append ? "ab" : "wb") != 0 || !f) {
            error = "cannot open temp file";
            return false;
        }
        std::string body; // unused for downloads
        uint64_t total = 0;
        long status = get_once(host, path, token, body, f, offset, total, progress, error);
        fclose(f);
        if (status == -1) {
            // Range ignored: restart without resume.
            std::remove(tmp_path.c_str());
            status = 0;
            error = "server ignored resume; retrying";
        }
        if (status == 200 || status == 206) {
            return true;
        }
        if (status == -2 || error == "cancelled") {
            error = "cancelled";
            return false;
        }
        if (attempt < retries) {
            const int shift = attempt < 3 ? attempt : 3;
            std::this_thread::sleep_for(std::chrono::seconds(1 << shift));
            continue;
        }
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

std::string HuggingFaceClient::ResolveUrl(const std::string& repo, const std::string& remote_path) {
    return "https://huggingface.co/" + repo + "/resolve/main/" + remote_path;
}

} // namespace studio
