#pragma once

#include "export/AnimationExporter.h"

namespace studio {

// Biovision Hierarchy exporter: joint tree + Euler motion channels.
// Imports into Blender, Unreal, Unity, Maya. Rotations as XYZ Euler degrees.
//
// BVH coordinate convention (single source of truth):
//   up=Y, forward=-Z, right=+X, right-handed, meters,
//   Euler order XYZ degrees, ROOT carries translation+rotation,
//   children carry rotation only. Internal Kimodo coords -> BVH via
//   prepareExport basis/scale in ONE place (ExportPreset.cpp). Exporter
//   itself does no axis swaps.
// Root motion: ROOT position changed, child offsets unchanged. No
// pelvis/hips duplication.
// FBX decision (plan section 30/phase 12): Autodesk FBX SDK is proprietary
// and cannot be vendored; GLB is the first-class interchange format and
// converts to FBX losslessly in Blender/Unreal/Unity. BVH covers the
// text-based DCC path instead. Main UE pipeline = BVH Humanoid export.
class BVHExporter : public AnimationExporter {
public:
    bool exportAnimation(const Animation& animation, const ExportOptions& options,
                         std::string& error) override;
    std::string lastReport() const override { return report_; }

private:
    std::string report_;
};

} // namespace studio
