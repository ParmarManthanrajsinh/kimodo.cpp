#pragma once

// Font Awesome Solid (fa-solid-900.ttf) icon codepoints as UTF-8.
// Requires FA font merged into ImGui atlas in Application::init.
// Every icon button pairs glyph + text/tooltip, so a missing glyph
// never removes meaning. Plain char pointers: MSVC char8_t stays
// out of ImGui's const char* API.
namespace studio::icons {

inline const char* kHome = reinterpret_cast<const char*>(u8"\uf015");
inline const char* kGenerate = reinterpret_cast<const char*>(u8"\uf0d0");
inline const char* kModel = reinterpret_cast<const char*>(u8"\uf1b2");
inline const char* kLibrary = reinterpret_cast<const char*>(u8"\uf008");
inline const char* kRetarget = reinterpret_cast<const char*>(u8"\uf0ec");
inline const char* kExport = reinterpret_cast<const char*>(u8"\uf093");
inline const char* kSettings = reinterpret_cast<const char*>(u8"\uf013");
inline const char* kPlay = reinterpret_cast<const char*>(u8"\uf04b");
inline const char* kPause = reinterpret_cast<const char*>(u8"\uf04c");
inline const char* kStop = reinterpret_cast<const char*>(u8"\uf04d");
inline const char* kFirst = reinterpret_cast<const char*>(u8"\uf049");
inline const char* kLast = reinterpret_cast<const char*>(u8"\uf050");
inline const char* kLoop = reinterpret_cast<const char*>(u8"\uf01e");
inline const char* kCamera = reinterpret_cast<const char*>(u8"\uf030");
inline const char* kGrid = reinterpret_cast<const char*>(u8"\uf00a");
inline const char* kAxes = reinterpret_cast<const char*>(u8"\uf05b");
inline const char* kFolder = reinterpret_cast<const char*>(u8"\uf07b");
inline const char* kSearch = reinterpret_cast<const char*>(u8"\uf002");
inline const char* kDownload = reinterpret_cast<const char*>(u8"\uf019");
inline const char* kCheck = reinterpret_cast<const char*>(u8"\uf00c");
inline const char* kWarn = reinterpret_cast<const char*>(u8"\uf071");
inline const char* kTrash = reinterpret_cast<const char*>(u8"\uf1f8");
inline const char* kSave = reinterpret_cast<const char*>(u8"\uf0c7");
inline const char* kEye = reinterpret_cast<const char*>(u8"\uf06e");
inline const char* kGpu = reinterpret_cast<const char*>(u8"\uf2db");
inline const char* kFloor = reinterpret_cast<const char*>(u8"\uf5fd"); // layer-group
inline const char* kSkeleton = reinterpret_cast<const char*>(u8"\uf5d7"); // bone
inline const char* kSplit = reinterpret_cast<const char*>(u8"\uf0db"); // columns
inline const char* kLink = reinterpret_cast<const char*>(u8"\uf0c1"); // link
inline const char* kExpand = reinterpret_cast<const char*>(u8"\uf065"); // expand-arrows-alt
inline const char* kReset = reinterpret_cast<const char*>(u8"\uf2f9"); // sync-alt
inline const char* kCheckCircle = reinterpret_cast<const char*>(u8"\uf058"); // check-circle
inline const char* kUser = reinterpret_cast<const char*>(u8"\uf007"); // user
inline const char* kRunning = reinterpret_cast<const char*>(u8"\uf70c"); // running
inline const char* kChevronDown = reinterpret_cast<const char*>(u8"\uf078"); // chevron-down
inline const char* kZoomIn = reinterpret_cast<const char*>(u8"\uf00e"); // search-plus
inline const char* kZoomOut = reinterpret_cast<const char*>(u8"\uf010"); // search-minus

// Merged FA glyph range for atlas build.
constexpr unsigned short kRangeFa[] = {0xf000, 0xf8ff, 0};

} // namespace studio::icons
