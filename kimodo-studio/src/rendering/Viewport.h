#pragma once

#include "character/CharacterAsset.h"
#include "character/SkinningData.h"
#include "rendering/GridRenderer.h"
#include "rendering/SkinningRenderer.h"
#include "raylib.h"

#include <string>
#include <vector>

namespace studio {

class FViewport {
public:
    void Reset();
    void Frame();
    void Update(bool mouseOverUi);
    void Draw3D();
    float GetDistance() const { return dist; }

    void SetPose(std::vector<Vector3> inPose, std::vector<int> inParents,
                 std::vector<std::string> inJointNames = {}) {
        pose = std::move(inPose);
        poseParents = std::move(inParents);
        jointNames = std::move(inJointNames);
    }
    bool HasPose() const { return !pose.empty(); }

    struct FDebugPose {
        std::vector<Vector3> pos;
        std::vector<int> parents;
        Vector3 offset = {0, 0, 0};
        Color joint = {140, 140, 150, 255};
        Color bone = {110, 110, 125, 255};
    };

    void SetDebugPoses(std::vector<FDebugPose> poses) { debug = std::move(poses); }
    void ClearDebug() { debug.clear(); }
    bool HasDebug() const { return !debug.empty(); }

    // Display Toggles
    void SetGrid(bool v) { bGridDraw = v; }
    void SetAxes(bool v) { bAxesDraw = v; }
    void SetFloor(bool v) { bFloorDraw = v; }
    void SetSkeleton(bool v) { bSkeletonDraw = v; }
    void SetCharacter(bool v) { bCharacterDraw = v; }
    void SetWireframe(bool v) { bWireframeDraw = v; }
    void SetBoneNames(bool v) { bBoneNamesDraw = v; }

    bool ShowGrid() const { return bGridDraw; }
    bool ShowAxes() const { return bAxesDraw; }
    bool ShowFloor() const { return bFloorDraw; }
    bool ShowSkeleton() const { return bSkeletonDraw; }
    bool ShowCharacter() const { return bCharacterDraw; }
    bool ShowWireframe() const { return bWireframeDraw; }
    bool ShowBoneNames() const { return bBoneNamesDraw; }

    // Active Character Asset & Skinning Data
    void SetCharacterAsset(FCharacterAsset* asset) { character = asset; }
    void SetCharacterSkinMatrices(std::vector<Matrix> inMatrices) {
        skinMatrices = std::move(inMatrices);
    }

    void SetProjection(int proj);
    int GetProjection() const { return projection; }

    void SetModelTransform(Vector3 pos, Vector3 rot, Vector3 scale) {
        modelPos = pos;
        modelRot = rot;
        modelScale = scale;
    }

    void CameraBasis(Vector3& right, Vector3& up) const;
    const Camera3D& GetCamera() const { return camera; }
    void DrawOrientationGizmo(float centerX, float centerY) const;

private:
    static void DrawPose(const std::vector<Vector3>& pose,
                         const std::vector<int>& parents, const Vector3& offset,
                         Color joint, Color bone);
    void drawBoneNames(const Camera3D& cam) const;
    void RecomputeCamera() const;

    Vector3 Target = {0, 1.0f, 0};
    float Yaw = 0.7f;
    float Pitch = 0.45f;
    float dist = 3.5f;
    mutable Camera3D camera{};
    FGridRenderer Grid;

    bool bGridDraw = true;
    bool bAxesDraw = true;
    bool bFloorDraw = true;
    bool bSkeletonDraw = true;
    bool bCharacterDraw = true;
    bool bWireframeDraw = false;
    bool bBoneNamesDraw = false;

    std::vector<Vector3> pose;
    std::vector<int> poseParents;
    std::vector<std::string> jointNames;
    std::vector<FDebugPose> debug;

    FCharacterAsset* character = nullptr;
    std::vector<Matrix> skinMatrices;
    FSkinningRenderer skinRenderer;

    int projection = 0; // 0 = Perspective, 1 = Orthographic
    Vector3 modelPos = {0.0f, 0.0f, 0.0f};
    Vector3 modelRot = {0.0f, 0.0f, 0.0f};
    Vector3 modelScale = {1.0f, 1.0f, 1.0f};
};

} // namespace studio
