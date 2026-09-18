#include "utils/FileDialog.h"

#include <nfd.h>

#include <cstdlib>

namespace studio {

bool FFileDialog::openFile(const char* filterList, const char* defaultPath, std::string& outPath) {
    nfdchar_t* out = nullptr;
    nfdresult_t result = NFD_OpenDialog(filterList, defaultPath, &out);
    if (result == NFD_OKAY && out) {
        outPath = out;
        free(out);
        return true;
    }
    return false;
}

bool FFileDialog::saveFile(const char* filterList, const char* defaultPath, std::string& outPath) {
    nfdchar_t* out = nullptr;
    nfdresult_t result = NFD_SaveDialog(filterList, defaultPath, &out);
    if (result == NFD_OKAY && out) {
        outPath = out;
        free(out);
        return true;
    }
    return false;
}

bool FFileDialog::pickFolder(const char* defaultPath, std::string& outPath) {
    nfdchar_t* out = nullptr;
    nfdresult_t result = NFD_PickFolder(defaultPath, &out);
    if (result == NFD_OKAY && out) {
        outPath = out;
        free(out);
        return true;
    }
    return false;
}

const char* FFileDialog::GetLastError() {
    return NFD_GetError();
}

} // namespace studio
