#pragma once

#include <string>

#include "animation/Animation.h"

namespace studio {

struct ExportOptions {
    std::string path;   // destination .glb file
    float fps = 0.0f;   // 0 = keep source fps
    float rootScale = 1.0f;
};

// Exporter interface (plan section 30): core animation system stays free
// of format-specific code; one class per format.
class AnimationExporter {
public:
    virtual ~AnimationExporter() = default;
    virtual bool exportAnimation(const Animation& animation,
                                 const ExportOptions& options,
                                 std::string& error) = 0;
};

} // namespace studio
