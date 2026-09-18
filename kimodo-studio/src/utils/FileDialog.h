#pragma once

#include <string>

namespace studio {

// Thin RAII-style wrapper over nativefiledialog (https://github.com/mlabbe/nativefiledialog).
// All methods are blocking: they run the OS-native modal dialog (IFileDialog on Windows,
// NSOpenPanel/NSSavePanel on macOS, GTK on Linux) and return once the user picks a file.
//
// Return value semantics:
//   true  -> user picked a path (outPath filled, UTF-8)
//   false -> user cancelled OR a programmatic error occurred (check lastError())
class FFileDialog {
public:
    // "Open file" dialog. filterList uses NFD format: comma-separated extensions,
    // semicolon between filters, e.g. "glb,gltf;bvh". Pass nullptr/"" for all files.
    static bool openFile(const char* filterList, const char* defaultPath, std::string& outPath);

    // "Save file" dialog. defaultPath may include a filename (e.g. "C:/exports/clip.bvh");
    // NFD parses the parent folder as the starting location.
    static bool saveFile(const char* filterList, const char* defaultPath, std::string& outPath);

    // "Select folder" dialog.
    static bool pickFolder(const char* defaultPath, std::string& outPath);

    // Human-readable error for the last NFD_ERROR result (empty otherwise).
    static const char* GetLastError();
};

} // namespace studio
