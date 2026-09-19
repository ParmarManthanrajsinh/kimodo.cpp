#pragma once

#include "export/AnimationExporter.h"

namespace studio {

// Skeleton-only GLB exporter: joint hierarchy as nodes (rest offsets as
// node translations), one animation clip with per-joint rotation channels
// plus root translation. No mesh: attach your own mesh to the imported
// joints in Blender/Unreal/Unity. Y-up data passes through unchanged.
class GLBExporter : public IAnimationExporter {
public:
    bool ExportAnimation(const Animation& animation, const ExportOptions& options, std::string& error) override;
    std::string GetLastReport() const override { return report; }

    static std::string DefaultExportDir();

private:
    std::string report;
};

} // namespace studio
