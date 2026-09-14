#include "utils/Hash.h"

#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#endif

namespace studio {

std::string FileHash::sha256(const std::string& path, std::string& error,
                             ProgressFn progress) {
#if defined(_WIN32)
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) {
        error = "cannot open file";
        return {};
    }
    // Total for progress.
    _fseeki64(f, 0, SEEK_END);
    const uint64_t total = static_cast<uint64_t>(_ftelli64(f));
    _fseeki64(f, 0, SEEK_SET);

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::string digest;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        error = "BCrypt provider failed";
        fclose(f);
        return {};
    }
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
        error = "BCrypt hash create failed";
        BCryptCloseAlgorithmProvider(alg, 0);
        fclose(f);
        return {};
    }
    std::vector<unsigned char> buf(1 << 20);
    uint64_t done = 0;
    for (;;) {
        const size_t n = fread(buf.data(), 1, buf.size(), f);
        if (n > 0) {
            if (BCryptHashData(hash, buf.data(), static_cast<ULONG>(n), 0) != 0) {
                error = "BCrypt hash failed";
                break;
            }
            done += n;
            if (progress) {
                progress(done, total);
            }
        }
        if (n < buf.size()) {
            if (std::feof(f)) {
                unsigned char out[32];
                if (BCryptFinishHash(hash, out, sizeof(out), 0) != 0) {
                    error = "BCrypt finish failed";
                } else {
                    char hex[65];
                    for (int i = 0; i < 32; ++i) {
                        std::snprintf(hex + i * 2, 3, "%02x", out[i]);
                    }
                    digest.assign(hex, 64);
                }
            } else {
                error = "file read failed";
            }
            break;
        }
    }
    if (hash) {
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    fclose(f);
    return digest;
#else
    (void)progress;
    error = "SHA-256 verify is Windows-only in this build";
    return {};
#endif
}

} // namespace studio
