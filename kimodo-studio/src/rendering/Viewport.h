#pragma once

#include <vector>

#include "raylib.h"
#include "rendering/GridRenderer.h"

namespace studio {

class Viewport {
public:
    void reset();
    void frame();
    void update(bool mouseOverUi);
    void draw3D() const;
    float distance() const { return dist_; }

    void setPose(std::vector<Vector3> pose, std::vector<int> parents) {
        pose_ = std::move(pose);
        poseParents_ = std::move(parents);
    }
    bool hasPose() const { return !pose_.empty(); }

    struct DebugPose {
        std::vector<Vector3> pos;
        std::vector<int> parents;
        Vector3 offset = {0, 0, 0};
        Color joint = {140, 140, 150, 255};
        Color bone = {110, 110, 125, 255};
    };
    // Side-by-side diagnostic figures (source / rest / retargeted).
    // Empty = normal single-pose rendering.
    void setDebugPoses(std::vector<DebugPose> poses) { debug_ = std::move(poses); }
    void clearDebug() { debug_.clear(); }
    bool hasDebug() const { return !debug_.empty(); }
    void setGrid(bool v) { gridDraw_ = v; }
    void setAxes(bool v) { axesDraw_ = v; }
    void setFloor(bool v) { floorDraw_ = v; }
    void setSkeleton(bool v) { skeletonDraw_ = v; }
    bool showGrid() const { return gridDraw_; }
    bool showAxes() const { return axesDraw_; }
    bool showFloor() const { return floorDraw_; }
    bool showSkeleton() const { return skeletonDraw_; }
    // Camera right/up in world space, for axis gizmo projection.
    void cameraBasis(Vector3& right, Vector3& up) const;

private:
    static void drawPose(const std::vector<Vector3>& pose,
                         const std::vector<int>& parents, const Vector3& offset,
                         Color joint, Color bone);
    void recomputeCamera() const;

    Vector3 target_ = {0, 1, 0};
    float yaw_ = 0.7f;
    float pitch_ = 0.45f;
    float dist_ = 8.0f;
    mutable Camera3D camera_ = {};
    GridRenderer grid_;
    bool gridDraw_ = true;
    bool axesDraw_ = true;
    bool floorDraw_ = true;
    bool skeletonDraw_ = true;
    bool grid() const { return gridDraw_; }
    bool axes() const { return axesDraw_; }
    std::vector<Vector3> pose_;
    std::vector<int> poseParents_;
    std::vector<DebugPose> debug_;
};

} // namespace studio
