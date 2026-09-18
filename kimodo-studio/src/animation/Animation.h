#pragma once

#include <array>
#include <string>
#include <vector>

#include "kimodo/KimodoAdapter.h" // MotionResult

namespace studio {

// Internal animation representation (plan section 21): central format
// for viewport, timeline, retargeting, exporters. Not GLB.
// Topology travels with the data so retargeted animations pose correctly.
struct FAnimation {
    int frames = 0;
    int joints = 0;
    float fps = 30.0f;
    std::string skeletonName = "soma30";
    std::vector<std::string> jointNames;
    std::vector<int> parents;
    std::vector<std::array<float, 3>> offsets;
    std::vector<float> localRotationsXyzw; // [frames, joints, 4]
    std::vector<float> rootPositions;      // [frames, 3]

    bool empty() const { return frames <= 0 || joints <= 0; }
    float GetDuration() const {
        return frames > 0 ? static_cast<float>(frames) / fps : 0.0f;
    }

    void fromMotionResult(const FMotionResult& m, float fpsValue = 30.0f);
};

} // namespace studio
