#pragma once

#include "character/CharacterAsset.h"
#include "raylib.h"
#include "rendering/GridRenderer.h"
#include "rendering/SkinningRenderer.h"

#include <string>
#include <vector>

namespace studio
{

class Viewport
{
public:
    void Reset();
    void Frame();
    void Update(bool mouse_over_ui);
    void Draw3D();
    float GetDistance() const
    {
        return dist;
    }

    void SetPose(std::vector<Vector3> in_pose, std::vector<int> in_parents,
                 std::vector<std::string> in_joint_names = {})
    {
        pose = std::move(in_pose);
        pose_parents = std::move(in_parents);
        joint_names = std::move(in_joint_names);
    }
    bool HasPose() const
    {
        return !pose.empty();
    }

    struct DebugPose
    {
        std::vector<Vector3> pos;
        std::vector<int> parents;
        Vector3 offset = {0, 0, 0};
        Color joint = {140, 140, 150, 255};
        Color bone = {110, 110, 125, 255};
    };

    void SetDebugPoses(std::vector<DebugPose> poses)
    {
        debug = std::move(poses);
    }
    void ClearDebug()
    {
        debug.clear();
    }
    bool HasDebug() const
    {
        return !debug.empty();
    }

    // Display Toggles
    void SetGrid(bool v)
    {
        draw_grid = v;
    }
    void SetAxes(bool v)
    {
        draw_axes = v;
    }
    void SetFloor(bool v)
    {
        draw_floor = v;
    }
    void SetSkeleton(bool v)
    {
        draw_skeleton = v;
    }
    void SetCharacter(bool v)
    {
        draw_character = v;
    }
    void SetWireframe(bool v)
    {
        draw_wireframe = v;
    }
    void SetBoneNames(bool v)
    {
        draw_bone_names = v;
    }

    bool ShowGrid() const
    {
        return draw_grid;
    }
    bool ShowAxes() const
    {
        return draw_axes;
    }
    bool ShowFloor() const
    {
        return draw_floor;
    }
    bool ShowSkeleton() const
    {
        return draw_skeleton;
    }
    bool ShowCharacter() const
    {
        return draw_character;
    }
    bool ShowWireframe() const
    {
        return draw_wireframe;
    }
    bool ShowBoneNames() const
    {
        return draw_bone_names;
    }

    // Active Character Asset & Skinning Data
    void SetCharacterAsset(CharacterAsset* asset)
    {
        character = asset;
    }
    void SetCharacterSkinMatrices(std::vector<Matrix> in_matrices)
    {
        skin_matrices = std::move(in_matrices);
    }

    void SetProjection(int proj);
    int GetProjection() const
    {
        return projection;
    }

    void SetModelTransform(Vector3 pos, Vector3 rot, Vector3 scale)
    {
        model_pos = pos;
        model_rot = rot;
        model_scale = scale;
    }

    void CameraBasis(Vector3& right, Vector3& up) const;
    const Camera3D& GetCamera() const
    {
        return camera;
    }
    void DrawOrientationGizmo(float center_x, float center_y) const;

private:
    static void DrawPose(const std::vector<Vector3>& pose, const std::vector<int>& parents, const Vector3& offset,
                         Color joint, Color bone);
    void DrawBoneNames(const Camera3D& cam) const;
    void RecomputeCamera() const;

    Vector3 target = {0, 1.0f, 0};
    float yaw = 0.7f;
    float pitch = 0.45f;
    float dist = 3.5f;
    mutable Camera3D camera{};
    GridRenderer grid;

    bool draw_grid = true;
    bool draw_axes = true;
    bool draw_floor = true;
    bool draw_skeleton = true;
    bool draw_character = true;
    bool draw_wireframe = false;
    bool draw_bone_names = false;

    std::vector<Vector3> pose;
    std::vector<int> pose_parents;
    std::vector<std::string> joint_names;
    std::vector<DebugPose> debug;

    CharacterAsset* character = nullptr;
    std::vector<Matrix> skin_matrices;
    SkinningRenderer skin_renderer;

    int projection = 0; // 0 = Perspective, 1 = Orthographic
    Vector3 model_pos = {0.0f, 0.0f, 0.0f};
    Vector3 model_rot = {0.0f, 0.0f, 0.0f};
    Vector3 model_scale = {1.0f, 1.0f, 1.0f};
};

} // namespace studio
