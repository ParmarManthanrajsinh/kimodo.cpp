#include "rendering/Viewport.h"
#include "animation/Skeleton.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace studio {

void FViewport::Reset() {
    Target = {0, 0.76f, 0};
    Yaw = 1.57f;
    Pitch = 0.05f;
    dist = 2.85f;
    RecomputeCamera();
}

void FViewport::Frame() {
    if (character && character->IsLoaded()) {
        const BoundingBox b = character->GetBounds();
        Target = Vector3Scale(Vector3Add(b.min, b.max), 0.5f);
        float diag = Vector3Distance(b.min, b.max);
        dist = std::max(2.0f, diag * 1.5f);
    } else if (!pose.empty()) {
        Vector3 sum{0, 0, 0};
        for (const auto& p : pose) sum = Vector3Add(sum, p);
        Target = Vector3Scale(sum, 1.0f / static_cast<float>(pose.size()));
        dist = 3.5f;
    } else {
        Target = {0, 1.0f, 0};
        dist = 3.5f;
    }
    RecomputeCamera();
}

void FViewport::Update(bool mouseOverUi) {
    const Vector2 delta = GetMouseDelta();

    // Camera owns mouse pointer only when ImGui does not capture it
    if (!mouseOverUi) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Yaw -= delta.x * 0.005f;
            Pitch -= delta.y * 0.005f;
            if (Pitch > 1.45f) Pitch = 1.45f;
            if (Pitch < -1.45f) Pitch = -1.45f;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
            (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && IsKeyDown(KEY_LEFT_SHIFT))) {
            Vector3 fwd = Vector3Normalize(Vector3Subtract(Target, camera.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera.up));
            Vector3 up = Vector3CrossProduct(right, fwd);
            const float s = dist * 0.0016f;
            Target = Vector3Subtract(Target, Vector3Scale(right, delta.x * s));
            Target = Vector3Add(Target, Vector3Scale(up, delta.y * s));
        }
    }

    const float wheel = mouseOverUi ? 0.0f : GetMouseWheelMove();
    if (wheel != 0.0f) {
        dist *= (wheel > 0) ? 0.9f : 1.1f;
        if (dist < 0.5f) dist = 0.5f;
        if (dist > 60.0f) dist = 60.0f;
    }

    if (IsKeyPressed(KEY_R)) Reset();
    if (IsKeyPressed(KEY_F)) Frame();

    RecomputeCamera();
}

void FViewport::SetProjection(int proj) {
    projection = proj;
    RecomputeCamera();
}

void FViewport::RecomputeCamera() const {
    const float cp = std::cos(Pitch);
    camera.position = {
        Target.x + dist * cp * std::cos(Yaw),
        Target.y + dist * std::sin(Pitch),
        Target.z + dist * cp * std::sin(Yaw),
    };
    camera.target = Target;
    camera.up = {0, 1, 0};
    camera.fovy = 45.0f;
    camera.projection = (projection == 1) ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
}

void FViewport::DrawPose(const std::vector<Vector3>& pose,
                         const std::vector<int>& parents, const Vector3& offset,
                         Color joint, Color bone) {
    const int J = static_cast<int>(pose.size());
    if (J == 0 || parents.size() != pose.size()) {
        return;
    }
    for (int j = 0; j < J; ++j) {
        const int p = parents[j];
        if (p >= 0 && p < J) {
            DrawCapsule(Vector3Add(pose[p], offset), Vector3Add(pose[j], offset),
                        0.025f, 6, 6, bone);
        }
    }
    for (int j = 0; j < J; ++j) {
        DrawSphere(Vector3Add(pose[j], offset), j == 0 ? 0.055f : 0.035f,
                   j == 0 ? YELLOW : joint);
    }
}

void FViewport::CameraBasis(Vector3& right, Vector3& up) const {
    Vector3 fwd = Vector3Normalize(Vector3Subtract(Target, camera.position));
    right = Vector3Normalize(Vector3CrossProduct(fwd, Vector3{0, 1, 0}));
    up = Vector3CrossProduct(right, fwd);
}

void FViewport::drawBoneNames(const Camera3D& cam) const {
    if (pose.empty() || jointNames.empty()) return;
    const int J = static_cast<int>(std::min(pose.size(), jointNames.size()));

    // Draw small billboard text for bones
    for (int j = 0; j < J; ++j) {
        Vector3 pos = Vector3Add(pose[j], Vector3{0, 0.04f, 0});
        // Project to screen or draw 3D billboard text
        DrawBillboard(cam, GetFontDefault().texture, pos, 0.05f, WHITE);
    }
}

void FViewport::Draw3D() {
    BeginMode3D(camera);

    if (bFloorDraw) {
        DrawPlane(Vector3{0, -0.005f, 0}, Vector2{40.0f, 40.0f}, Color{14, 14, 18, 200});
    }

    Grid.Draw(bGridDraw, bAxesDraw);

    // Side-by-side debug poses
    if (!debug.empty()) {
        for (const FDebugPose& d : debug) {
            DrawPose(d.pos, d.parents, d.offset, d.joint, d.bone);
        }
        EndMode3D();
        return;
    }

    // Model transform application
    bool hasModelTransform = (modelPos.x != 0.0f || modelPos.y != 0.0f || modelPos.z != 0.0f ||
                              modelRot.x != 0.0f || modelRot.y != 0.0f || modelRot.z != 0.0f ||
                              modelScale.x != 1.0f || modelScale.y != 1.0f || modelScale.z != 1.0f);
    if (hasModelTransform) {
        rlPushMatrix();
        rlTranslatef(modelPos.x, modelPos.y, modelPos.z);
        rlRotatef(modelRot.x, 1, 0, 0);
        rlRotatef(modelRot.y, 0, 1, 0);
        rlRotatef(modelRot.z, 0, 0, 1);
        rlScalef(modelScale.x, modelScale.y, modelScale.z);
    }

    // 1. Draw Skinned 3D Character Mesh
    if (bCharacterDraw && character && character->IsLoaded()) {
        skinRenderer.drawCharacter(*character, skinMatrices, bWireframeDraw);
    }

    // 2. Draw Skeleton Bones & Joints (X-Ray overlay through mesh matching Image 2)
    if (bSkeletonDraw) {
        const int J = static_cast<int>(pose.size());
        const Color colBoneGhost = Color{95, 175, 235, 140};
        const Color colJointGhost = Color{145, 195, 245, 180};
        const Color colBoneSolid = Color{95, 175, 235, 240};
        const Color colJointSolid = Color{145, 195, 245, 255};

        if (J > 0 && poseParents.size() == pose.size()) {
            // Pass 1: X-Ray overlay visible through character mesh
            rlDisableDepthTest();
            for (int j = 0; j < J; ++j) {
                const int p = poseParents[j];
                if (p >= 0 && p < J) {
                    DrawCapsule(pose[p], pose[j], 0.018f, 8, 8, colBoneGhost);
                }
            }
            for (int j = 0; j < J; ++j) {
                DrawSphere(pose[j], 0.024f, colJointGhost);
            }
            rlEnableDepthTest();

            // Pass 2: Solid depth-tested pass
            for (int j = 0; j < J; ++j) {
                const int p = poseParents[j];
                if (p >= 0 && p < J) {
                    DrawCapsule(pose[p], pose[j], 0.018f, 8, 8, colBoneSolid);
                }
            }
            for (int j = 0; j < J; ++j) {
                DrawSphere(pose[j], 0.024f, colJointSolid);
            }
        } else if (!bCharacterDraw || !character || !character->IsLoaded()) {
            // Standby origin rig stub
            DrawSphere(Vector3{0, 1.0f, 0}, 0.045f, colJointSolid);
            DrawSphere(Vector3{0, 1.6f, 0}, 0.035f, colJointSolid);
            DrawCapsule(Vector3{0, 0.4f, 0}, Vector3{0, 1.6f, 0}, 0.018f, 8, 8, colBoneSolid);
        }
    }

    // 3. Draw Bone Names if enabled
    if (bBoneNamesDraw) {
        drawBoneNames(camera);
    }

    if (hasModelTransform) {
        rlPopMatrix();
    }

    EndMode3D();
}

void FViewport::DrawOrientationGizmo(float centerX, float centerY) const {
    Vector3 fwd = Vector3Normalize(Vector3Subtract(Target, camera.position));
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
        {'Z', Vector3{0, 0, 1}, Color{60, 130, 245, 255}, Vector3DotProduct(fwd, Vector3{0, 0, 1})}
    };
    std::sort(axes.begin(), axes.end(), [](const AxisInfo& a, const AxisInfo& b) {
        return a.depth < b.depth;
    });

    const float axisLength = 22.0f;
    for (const auto& ax : axes) {
        float sx = Vector3DotProduct(ax.dir, right) * axisLength;
        float sy = -Vector3DotProduct(ax.dir, up) * axisLength;
        Vector2 endPt = {centerX + sx, centerY + sy};
        DrawLineEx(Vector2{centerX, centerY}, endPt, 2.0f, ax.col);
        char str[2] = {ax.label, '\0'};
        DrawText(str, static_cast<int>(endPt.x + (sx >= 0 ? 3 : -8)), static_cast<int>(endPt.y + (sy >= 0 ? 2 : -10)), 12, ax.col);
    }
}

} // namespace studio
