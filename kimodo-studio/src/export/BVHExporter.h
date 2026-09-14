#pragma once

#include "export/AnimationExporter.h"

namespace studio {

// Biovision Hierarchy exporter: joint tree + Euler motion channels.
// Imports into Blender, Unreal, Unity, Maya. Rotations as XYZ Euler degrees.
// FBX decision (plan section 30/phase 12): Autodesk FBX SDK is proprietary
// and cannot be vendored; GLB is the first-class interchange format and
// converts to FBX losslessly in Blender/Unreal/Unity. BVH covers the
// text-based DCC path instead.
class BVHExporter : public AnimationExporter {
public:
    bool exportAnimation(const Animation& animation, const ExportOptions& options,
                         std::string& error) override;
    std::string lastReport() const override { return report_; }

private:
    std::string report_;
};

} // namespace studio
