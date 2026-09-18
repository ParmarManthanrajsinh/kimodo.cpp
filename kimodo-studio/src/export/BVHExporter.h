#pragma once

#include "export/AnimationExporter.h"

namespace studio {

class FBVHExporter : public IAnimationExporter {
public:
    bool ExportAnimation(const FAnimation& animation, const FExportOptions& options,
                         std::string& error) override;
    std::string GetLastReport() const override { return report; }

    // Export with optional frame range (startFrame to endFrame, -1 for full)
    static bool ExportWithRange(const FAnimation& animation, const FExportOptions& options,
                                int startFrame, int endFrame, std::string& error,
                                std::string* report = nullptr);

    // Convert quaternion (xyzw) to deterministic XYZ Euler degrees
    static void QuatToEulerXYZ(float x, float y, float z, float w,
                               float& ex, float& ey, float& ez);

private:
    std::string report;
};

} // namespace studio
