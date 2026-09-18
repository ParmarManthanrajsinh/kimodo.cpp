#include "huggingface/HFAuthenticator.h"

#include "huggingface/HuggingFaceClient.h"

#if defined(_WIN32)
#include <windows.h>
#include <wincred.h>
#endif

namespace studio {

const char* FHFAuthenticator::credTarget() {
    return "KimodoStudio/HuggingFace";
}

bool FHFAuthenticator::SaveToken(const std::string& token, std::string& error) {
#if defined(_WIN32)
    if (token.empty() || token.size() > 4096) {
        error = "invalid token size";
        return false;
    }
    CREDENTIALA cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<char*>(credTarget());
    cred.CredentialBlobSize = static_cast<DWORD>(token.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(token.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteA(&cred, 0)) {
        error = "credential store write failed";
        return false;
    }
    return true;
#else
    (void)token;
    error = "secure storage is Windows-only in this build";
    return false;
#endif
}

bool FHFAuthenticator::loadToken(std::string& token) {
#if defined(_WIN32)
    PCREDENTIALA cred = nullptr;
    if (!CredReadA(credTarget(), CRED_TYPE_GENERIC, 0, &cred)) {
        return false;
    }
    token.assign(reinterpret_cast<char*>(cred->CredentialBlob), cred->CredentialBlobSize);
    SecureZeroMemory(cred->CredentialBlob, cred->CredentialBlobSize);
    CredFree(cred);
    return !token.empty();
#else
    (void)token;
    return false;
#endif
}

bool FHFAuthenticator::clearToken() {
#if defined(_WIN32)
    return CredDeleteA(credTarget(), CRED_TYPE_GENERIC, 0) != FALSE;
#else
    return false;
#endif
}

std::string FHFAuthenticator::validate(const std::string& token, std::string& error) {
    return FHuggingFaceClient::whoami(token, error);
}

} // namespace studio
