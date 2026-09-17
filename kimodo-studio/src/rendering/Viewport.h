#pragma once

#include "character/CharacterAsset.h"
#include "character/SkinningData.h"
#include "rendering/GridRenderer.h"
#include "rendering/SkinningRenderer.h"
#include "raylib.h"

#include <string>
#include <vector>

namespace studio {

class Viewport {
public:
    void reset();
    void frame();
    void update(bool mouseOverUi);
    void draw3D();
    float distance() const { return dist_; }

    void setPose(std::vector<Vector3> pose, std::vector<int> parents,
                 std::vector<std::string> jointNames = {}) {
        pose_ = std::move(pose);
        poseParents_ = std::move(parents);
        jointNames_ = std::move(jointNames);
    }
    bool hasPose() const { return !pose_.empty(); }

    struct DebugPose {
        std::vector<Vector3> pos;
        std::vector<int> parents;
        Vector3 offset = {0, 0, 0};
        Color joint = {140, 140, 150, 255};
        Color bone = {110, 110, 125, 255};
    };

    void setDebugPoses(std::vector<DebugPose> poses) { debug_ = std::move(poses); }
    void clearDebug() { debug_.clear(); }
    bool hasDebug() const { return !debug_.empty(); }

    // Display Toggles
    void setGrid(bool v) { gridDraw_ = v; }
    void setAxes(bool v) { axesDraw_ = v; }
    void setFloor(bool v) { floorDraw_ = v; }
    void setSkeleton(bool v) { skeletonDraw_ = v; }
    void setCharacter(bool v) { characterDraw_ = v; }
    void setWireframe(bool v) { wireframeDraw_ = v; }
    void setBoneNames(bool v) { boneNamesDraw_ = v; }

    bool showGrid() const { return gridDraw_; }
    bool showAxes() const { return axesDraw_; }
    bool showFloor() const { return floorDraw_; }
    bool showSkeleton() const { return skeletonDraw_; }
    bool showCharacter() const { return characterDraw_; }
    bool showWireframe() const { return wireframeDraw_; }
    bool showBoneNames() const { return boneNamesDraw_; }

    // Active Character Asset & Skinning Data
    void setCharacterAsset(CharacterAsset* asset) { character_ = asset; }
    void setCharacterSkinMatrices(std::vector<Matrix> skinMatrices) {
        skinMatrices_ = std::move(skinMatrices);
    }

    void setProjection(int proj);
    int projection() const { return projection_; }

    void setModelTransform(Vector3 pos, Vector3 rot, Vector3 scale) {
        modelPos_ = pos;
        modelRot_ = rot;
        modelScale_ = scale;
    }

    void cameraBasis(Vector3& right, Vector3& up) const;
    const Camera3D& camera() const { return camera_; }
    void drawOrientationGizmo(float centerX, float centerY) const;

private:
    static void drawPose(const std::vector<Vector3>& pose,
                         const std::vector<int>& parents, const Vector3& offset,
                         Color joint, Color bone);
    void drawBoneNames(const Camera3D& cam) const;
    void recomputeCamera() const;

    Vector3 target_ = {0, 1.0f, 0};
    float yaw_ = 0.7f;
    float pitch_ = 0.45f;
    float dist_ = 3.5f;
    mutable Camera3D camera_{};
    GridRenderer grid_;

    bool gridDraw_ = true;
    bool axesDraw_ = true;
    bool floorDraw_ = true;
    bool skeletonDraw_ = true;
    bool characterDraw_ = true;
    bool wireframeDraw_ = false;
    bool boneNamesDraw_ = false;

    std::vector<Vector3> pose_;
    std::vector<int> poseParents_;
    std::vector<std::string> jointNames_;
    std::vector<DebugPose> debug_;

    CharacterAsset* character_ = nullptr;
    std::vector<Matrix> skinMatrices_;
    SkinningRenderer skinRenderer_;

    int projection_ = 0; // 0 = Perspective, 1 = Orthographic
    Vector3 modelPos_ = {0.0f, 0.0f, 0.0f};
    Vector3 modelRot_ = {0.0f, 0.0f, 0.0f};
    Vector3 modelScale_ = {1.0f, 1.0f, 1.0f};
};

} // namespace studio
