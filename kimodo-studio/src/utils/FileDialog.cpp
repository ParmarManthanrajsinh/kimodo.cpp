#include "utils/FileDialog.h"

#include <nfd.h>

#include <cstdlib>

namespace studio {

bool FileDialog::OpenFile(const char* filter_list, const char* default_path, std::string& out_path) {
    nfdchar_t* out = nullptr;
    nfdresult_t result = NFD_OpenDialog(filter_list, default_path, &out);
    if (result == NFD_OKAY && out) {
        out_path = out;
        free(out);
        return true;
    }
    return false;
}

bool FileDialog::SaveFile(const char* filter_list, const char* default_path, std::string& out_path) {
    nfdchar_t* out = nullptr;
    nfdresult_t result = NFD_SaveDialog(filter_list, default_path, &out);
    if (result == NFD_OKAY && out) {
        out_path = out;
        free(out);
        return true;
    }
    return false;
}

bool FileDialog::PickFolder(const char* default_path, std::string& out_path) {
    nfdchar_t* out = nullptr;
    nfdresult_t result = NFD_PickFolder(default_path, &out);
    if (result == NFD_OKAY && out) {
        out_path = out;
        free(out);
        return true;
    }
    return false;
}

const char* FileDialog::GetLastError() { return NFD_GetError(); }

} // namespace studio
