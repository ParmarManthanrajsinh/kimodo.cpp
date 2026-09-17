#pragma once

#include "export/AnimationExporter.h"

namespace studio {

class BVHExporter : public AnimationExporter {
public:
    bool exportAnimation(const Animation& animation, const ExportOptions& options,
                         std::string& error) override;
    std::string lastReport() const override { return report_; }

    // Export with optional frame range (startFrame to endFrame, -1 for full)
    static bool exportWithRange(const Animation& animation, const ExportOptions& options,
                                int startFrame, int endFrame, std::string& error,
                                std::string* report = nullptr);

    // Convert quaternion (xyzw) to deterministic XYZ Euler degrees
    static void quatToEulerXYZ(float x, float y, float z, float w,
                               float& ex, float& ey, float& ez);

private:
    std::string report_;
};

} // namespace studio
