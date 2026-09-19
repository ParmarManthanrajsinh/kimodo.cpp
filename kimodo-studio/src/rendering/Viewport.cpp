#include "rendering/Viewport.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace studio {

void Viewport::Reset() {
    target = {0, 0.76f, 0};
    yaw = 1.57f;
    pitch = 0.05f;
    dist = 2.85f;
    RecomputeCamera();
}

void Viewport::Frame() {
    if (character && character->IsLoaded()) {
        const BoundingBox b = character->GetBounds();
        target = Vector3Scale(Vector3Add(b.min, b.max), 0.5f);
        float diag = Vector3Distance(b.min, b.max);
        dist = std::max(2.0f, diag * 1.5f);
    } else if (!pose.empty()) {
        Vector3 sum{0, 0, 0};
        for (const auto& p : pose)
            sum = Vector3Add(sum, p);
        target = Vector3Scale(sum, 1.0f / static_cast<float>(pose.size()));
        dist = 3.5f;
    } else {
        target = {0, 1.0f, 0};
        dist = 3.5f;
    }
    RecomputeCamera();
}

void Viewport::Update(bool mouse_over_ui) {
    const Vector2 delta = GetMouseDelta();

    // Camera owns mouse pointer only when ImGui does not capture it
    if (!mouse_over_ui) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            yaw -= delta.x * 0.005f;
            pitch -= delta.y * 0.005f;
            if (pitch > 1.45f)
                pitch = 1.45f;
            if (pitch < -1.45f)
                pitch = -1.45f;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
            (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && IsKeyDown(KEY_LEFT_SHIFT))) {
            Vector3 fwd = Vector3Normalize(Vector3Subtract(target, camera.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera.up));
            Vector3 up = Vector3CrossProduct(right, fwd);
            const float s = dist * 0.0016f;
            target = Vector3Subtract(target, Vector3Scale(right, delta.x * s));
            target = Vector3Add(target, Vector3Scale(up, delta.y * s));
        }
    }

    const float wheel = mouse_over_ui ? 0.0f : GetMouseWheelMove();
    if (wheel != 0.0f) {
        dist *= (wheel > 0) ? 0.9f : 1.1f;
        if (dist < 0.5f)
            dist = 0.5f;
        if (dist > 60.0f)
            dist = 60.0f;
    }

    if (IsKeyPressed(KEY_R))
        Reset();
    if (IsKeyPressed(KEY_F))
        Frame();

    RecomputeCamera();
}

void Viewport::SetProjection(int proj) {
    projection = proj;
    RecomputeCamera();
}

void Viewport::RecomputeCamera() const {
    const float cp = std::cos(pitch);
    camera.position = {
        target.x + dist * cp * std::cos(yaw),
        target.y + dist * std::sin(pitch),
        target.z + dist * cp * std::sin(yaw),
    };
    camera.target = target;
    camera.up = {0, 1, 0};
    camera.fovy = 45.0f;
    camera.projection = (projection == 1) ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
}

void Viewport::DrawPose(const std::vector<Vector3>& pose, const std::vector<int>& parents, const Vector3& offset,
                        Color joint, Color bone) {
    const int J = static_cast<int>(pose.size());
    if (J == 0 || parents.size() != pose.size()) {
        return;
    }
    for (int j = 0; j < J; ++j) {
        const int p = parents[j];
        if (p >= 0 && p < J) {
            DrawCapsule(Vector3Add(pose[p], offset), Vector3Add(pose[j], offset), 0.025f, 6, 6, bone);
        }
    }
    for (int j = 0; j < J; ++j) {
        DrawSphere(Vector3Add(pose[j], offset), j == 0 ? 0.055f : 0.035f, j == 0 ? YELLOW : joint);
    }
}

void Viewport::CameraBasis(Vector3& right, Vector3& up) const {
    Vector3 fwd = Vector3Normalize(Vector3Subtract(target, camera.position));
    right = Vector3Normalize(Vector3CrossProduct(fwd, Vector3{0, 1, 0}));
    up = Vector3CrossProduct(right, fwd);
}

void Viewport::DrawBoneNames(const Camera3D& cam) const {
    if (pose.empty() || joint_names.empty())
        return;
    const int J = static_cast<int>(std::min(pose.size(), joint_names.size()));

    // Draw small billboard text for bones
    for (int j = 0; j < J; ++j) {
        Vector3 pos = Vector3Add(pose[j], Vector3{0, 0.04f, 0});
        // Project to screen or draw 3D billboard text
        DrawBillboard(cam, GetFontDefault().texture, pos, 0.05f, WHITE);
    }
}

void Viewport::Draw3D() {
    BeginMode3D(camera);

    if (draw_floor) {
        DrawPlane(Vector3{0, -0.005f, 0}, Vector2{40.0f, 40.0f}, Color{14, 14, 18, 200});
    }

    grid.Draw(draw_grid, draw_axes);

    // Side-by-side debug poses
    if (!debug.empty()) {
        for (const DebugPose& d : debug) {
            DrawPose(d.pos, d.parents, d.offset, d.joint, d.bone);
        }
        EndMode3D();
        return;
    }

    // Model transform application
    bool has_model_transform = (model_pos.x != 0.0f || model_pos.y != 0.0f || model_pos.z != 0.0f ||
                                model_rot.x != 0.0f || model_rot.y != 0.0f || model_rot.z != 0.0f ||
                                model_scale.x != 1.0f || model_scale.y != 1.0f || model_scale.z != 1.0f);
    if (has_model_transform) {
        rlPushMatrix();
        rlTranslatef(model_pos.x, model_pos.y, model_pos.z);
        rlRotatef(model_rot.x, 1, 0, 0);
        rlRotatef(model_rot.y, 0, 1, 0);
        rlRotatef(model_rot.z, 0, 0, 1);
        rlScalef(model_scale.x, model_scale.y, model_scale.z);
    }

    // 1. Draw Skinned 3D Character Mesh
    if (draw_character && character && character->IsLoaded()) {
        skin_renderer.DrawCharacter(*character, skin_matrices, draw_wireframe);
    }

    // 2. Draw Skeleton Bones & Joints (X-Ray overlay through mesh matching Image 2)
    if (draw_skeleton) {
        const int J = static_cast<int>(pose.size());
        const Color col_bone_ghost = Color{95, 175, 235, 140};
        const Color col_joint_ghost = Color{145, 195, 245, 180};
        const Color col_bone_solid = Color{95, 175, 235, 240};
        const Color col_joint_solid = Color{145, 195, 245, 255};

        if (J > 0 && pose_parents.size() == pose.size()) {
            // Pass 1: X-Ray overlay visible through character mesh
            rlDisableDepthTest();
            for (int j = 0; j < J; ++j) {
                const int p = pose_parents[j];
                if (p >= 0 && p < J) {
                    DrawCapsule(pose[p], pose[j], 0.018f, 8, 8, col_bone_ghost);
                }
            }
            for (int j = 0; j < J; ++j) {
                DrawSphere(pose[j], 0.024f, col_joint_ghost);
            }
            rlEnableDepthTest();

            // Pass 2: Solid depth-tested pass
            for (int j = 0; j < J; ++j) {
                const int p = pose_parents[j];
                if (p >= 0 && p < J) {
                    DrawCapsule(pose[p], pose[j], 0.018f, 8, 8, col_bone_solid);
                }
            }
            for (int j = 0; j < J; ++j) {
                DrawSphere(pose[j], 0.024f, col_joint_solid);
            }
        } else if (!draw_character || !character || !character->IsLoaded()) {
            // Standby origin rig stub
            DrawSphere(Vector3{0, 1.0f, 0}, 0.045f, col_joint_solid);
            DrawSphere(Vector3{0, 1.6f, 0}, 0.035f, col_joint_solid);
            DrawCapsule(Vector3{0, 0.4f, 0}, Vector3{0, 1.6f, 0}, 0.018f, 8, 8, col_bone_solid);
        }
    }

    // 3. Draw Bone Names if enabled
    if (draw_bone_names) {
        DrawBoneNames(camera);
    }

    if (has_model_transform) {
        rlPopMatrix();
    }

    EndMode3D();
}

void Viewport::DrawOrientationGizmo(float center_x, float center_y) const {
    Vector3 fwd = Vector3Normalize(Vector3Subtract(target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, Vector3{0, 1, 0}));
    Vector3 up = Vector3CrossProduct(right, fwd);

    struct AxisInfo {
        char label;
        Vector3 dir;
        Color col;
        float depth;
    };
    std::vector<AxisInfo> axes = {
        {'X', Vector3{1, 0, 0}, Color{235, 60, 60, 255}, Vector3DotProduct(fwd, Vector3{1, 0, 0})},
        {'Y', Vector3{0, 1, 0}, Color{45, 200, 85, 255}, Vector3DotProduct(fwd, Vector3{0, 1, 0})},
        {'Z', Vector3{0, 0, 1}, Color{60, 130, 245, 255}, Vector3DotProduct(fwd, Vector3{0, 0, 1})}};
    std::sort(axes.begin(), axes.end(), [](const AxisInfo& a, const AxisInfo& b) { return a.depth < b.depth; });

    const float axis_length = 22.0f;
    for (const auto& ax : axes) {
        float sx = Vector3DotProduct(ax.dir, right) * axis_length;
        float sy = -Vector3DotProduct(ax.dir, up) * axis_length;
        Vector2 end_pt = {center_x + sx, center_y + sy};
        DrawLineEx(Vector2{center_x, center_y}, end_pt, 2.0f, ax.col);
        char str[2] = {ax.label, '\0'};
        DrawText(str, static_cast<int>(end_pt.x + (sx >= 0 ? 3 : -8)), static_cast<int>(end_pt.y + (sy >= 0 ? 2 : -10)),
                 12, ax.col);
    }
}

} // namespace studio
