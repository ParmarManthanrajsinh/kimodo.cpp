#pragma once

#include "export/AnimationExporter.h"

namespace studio
{

class BVHExporter : public IAnimationExporter
{
public:
    bool ExportAnimation(const Animation& animation, const ExportOptions& options, std::string& error) override;
    std::string GetLastReport() const override
    {
        return report;
    }

    // Export with optional frame range (startFrame to endFrame, -1 for full)
    static bool ExportWithRange(const Animation& animation, const ExportOptions& options, int start_frame,
                                int end_frame, std::string& error, std::string* report = nullptr);

    // Convert quaternion (xyzw) to deterministic XYZ Euler degrees
    static void QuatToEulerXYZ(float x, float y, float z, float w, float& ex, float& ey, float& ez);

private:
    std::string report;
};

} // namespace studio
