#include "huggingface/HFAuthenticator.h"

#include "huggingface/HuggingFaceClient.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

namespace studio
{

const char* HFAuthenticator::CredTarget()
{
    return "KimodoStudio/HuggingFace";
}

bool HFAuthenticator::SaveToken(const std::string& token, std::string& error)
{
#if defined(_WIN32)
    if (token.empty() || token.size() > 4096)
    {
        error = "invalid token size";
        return false;
    }
    CREDENTIALA cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<char*>(CredTarget());
    cred.CredentialBlobSize = static_cast<DWORD>(token.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(token.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteA(&cred, 0))
    {
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

bool HFAuthenticator::LoadToken(std::string& token)
{
#if defined(_WIN32)
    PCREDENTIALA cred = nullptr;
    if (!CredReadA(CredTarget(), CRED_TYPE_GENERIC, 0, &cred))
    {
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

bool HFAuthenticator::ClearToken()
{
#if defined(_WIN32)
    return CredDeleteA(CredTarget(), CRED_TYPE_GENERIC, 0) != FALSE;
#else
    return false;
#endif
}

std::string HFAuthenticator::Validate(const std::string& token, std::string& error)
{
    return HuggingFaceClient::Whoami(token, error);
}

} // namespace studio
