#pragma once

#include <string>

#include "animation/Animation.h"
#include "export/ExportPreset.h"

namespace studio {

struct FExportOptions {
    std::string path;   // destination .glb file
    float fps = 0.0f;   // 0 = keep source fps
    float rootScale = 1.0f;
    Mat3 basis = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}}; // applied to positions/rotations
    ERootMotion rootMotion = ERootMotion::Preserve;
};

// Exporter interface (plan section 30): core animation system stays free
// of format-specific code; one class per format.
class IAnimationExporter {
public:
    virtual ~IAnimationExporter() = default;
    virtual bool ExportAnimation(const FAnimation& animation,
                                 const FExportOptions& options,
                                 std::string& error) = 0;
    virtual std::string GetLastReport() const { return {}; }
};

} // namespace studio
