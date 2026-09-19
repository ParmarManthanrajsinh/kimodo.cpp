#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "raylib.h"

// SOMA-30 rest skeleton. Joint names, parents and offsets mirror
// kimodo src/skeleton.hpp (soma30_spec, data from NVIDIA Kimodo).
// Studio keeps its own copy so the UI never reaches into kimodo sources.
namespace studio {

inline constexpr int kSomaJoints = 30;

struct Soma30Spec {
    static std::array<std::string_view, kSomaJoints> names;
    static std::array<int, kSomaJoints> parents;
    static std::array<std::array<float, 3>, kSomaJoints> offsets;
};

// Forward kinematics: local quats [J,4] xyzw (parent-relative) + root
// translation -> world positions. Matches kimodo decode convention:
// world(j) = world(parent) * local(j), root world pos = rootPositions.
class Skeleton {
public:
    // local_xyzw: joints*4 floats, root: 3 floats. out: joints Vector3.
    static void ForwardKinematics(const float* local_xyzw, const float* root, std::vector<Vector3>& out);
    static void ForwardKinematicsGeneral(const float* local_xyzw, const float* root, const std::vector<int>& parents,
                                         const std::vector<std::array<float, 3>>& offsets, std::vector<Vector3>& out);
    // Same, but also returns world orientations (parent-composed local quats).
    static void ForwardKinematicsFull(const float* local_xyzw, const float* root, const std::vector<int>& parents,
                                      const std::vector<std::array<float, 3>>& offsets, std::vector<Vector3>& out_pos,
                                      std::vector<Quaternion>& out_rot);
    static const std::array<int, kSomaJoints>& parents() { return Soma30Spec::parents; }
};

} // namespace studio
