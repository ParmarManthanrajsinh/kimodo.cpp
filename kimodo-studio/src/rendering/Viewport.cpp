#include "rendering/Viewport.h"
#include "animation/Skeleton.h"
#include "raymath.h"
#include "rlgl.h"

#include <cmath>

namespace studio {

void Viewport::reset() {
    target_ = {0, 1.0f, 0};
    yaw_ = 0.7f;
    pitch_ = 0.35f;
    dist_ = 3.5f;
    recomputeCamera();
}

void Viewport::frame() {
    if (character_ && character_->isLoaded()) {
        const BoundingBox b = character_->bounds();
        target_ = Vector3Scale(Vector3Add(b.min, b.max), 0.5f);
        float diag = Vector3Distance(b.min, b.max);
        dist_ = std::max(2.0f, diag * 1.5f);
    } else if (!pose_.empty()) {
        Vector3 sum{0, 0, 0};
        for (const auto& p : pose_) sum = Vector3Add(sum, p);
        target_ = Vector3Scale(sum, 1.0f / static_cast<float>(pose_.size()));
        dist_ = 3.5f;
    } else {
        target_ = {0, 1.0f, 0};
        dist_ = 3.5f;
    }
    recomputeCamera();
}

void Viewport::update(bool mouseOverUi) {
    const Vector2 delta = GetMouseDelta();

    // Camera owns mouse pointer only when ImGui does not capture it
    if (!mouseOverUi) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            yaw_ -= delta.x * 0.005f;
            pitch_ -= delta.y * 0.005f;
            if (pitch_ > 1.45f) pitch_ = 1.45f;
            if (pitch_ < -1.45f) pitch_ = -1.45f;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ||
            (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && IsKeyDown(KEY_LEFT_SHIFT))) {
            Vector3 fwd = Vector3Normalize(Vector3Subtract(target_, camera_.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera_.up));
            Vector3 up = Vector3CrossProduct(right, fwd);
            const float s = dist_ * 0.0016f;
            target_ = Vector3Subtract(target_, Vector3Scale(right, delta.x * s));
            target_ = Vector3Add(target_, Vector3Scale(up, delta.y * s));
        }
    }

    const float wheel = mouseOverUi ? 0.0f : GetMouseWheelMove();
    if (wheel != 0.0f) {
        dist_ *= (wheel > 0) ? 0.9f : 1.1f;
        if (dist_ < 0.5f) dist_ = 0.5f;
        if (dist_ > 60.0f) dist_ = 60.0f;
    }

    if (IsKeyPressed(KEY_R)) reset();
    if (IsKeyPressed(KEY_F)) frame();

    recomputeCamera();
}

void Viewport::recomputeCamera() const {
    const float cp = std::cos(pitch_);
    camera_.position = {
        target_.x + dist_ * cp * std::cos(yaw_),
        target_.y + dist_ * std::sin(pitch_),
        target_.z + dist_ * cp * std::sin(yaw_),
    };
    camera_.target = target_;
    camera_.up = {0, 1, 0};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void Viewport::drawPose(const std::vector<Vector3>& pose,
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

void Viewport::cameraBasis(Vector3& right, Vector3& up) const {
    Vector3 fwd = Vector3Normalize(Vector3Subtract(target_, camera_.position));
    right = Vector3Normalize(Vector3CrossProduct(fwd, Vector3{0, 1, 0}));
    up = Vector3CrossProduct(right, fwd);
}

void Viewport::drawBoneNames(const Camera3D& cam) const {
    if (pose_.empty() || jointNames_.empty()) return;
    const int J = static_cast<int>(std::min(pose_.size(), jointNames_.size()));

    // Draw small billboard text for bones
    for (int j = 0; j < J; ++j) {
        Vector3 pos = Vector3Add(pose_[j], Vector3{0, 0.04f, 0});
        // Project to screen or draw 3D billboard text
        DrawBillboard(cam, GetFontDefault().texture, pos, 0.05f, WHITE);
    }
}

void Viewport::draw3D() {
    BeginMode3D(camera_);

    if (floorDraw_) {
        DrawPlane(Vector3{0, -0.005f, 0}, Vector2{40.0f, 40.0f}, Color{14, 14, 18, 200});
    }

    grid_.draw(gridDraw_, axesDraw_);

    // Side-by-side debug poses
    if (!debug_.empty()) {
        for (const DebugPose& d : debug_) {
            drawPose(d.pos, d.parents, d.offset, d.joint, d.bone);
        }
        EndMode3D();
        return;
    }

    // 1. Draw Skinned 3D Character Mesh
    if (characterDraw_ && character_ && character_->isLoaded()) {
        skinRenderer_.drawCharacter(*character_, skinMatrices_, wireframeDraw_);
    }

    // 2. Draw Skeleton Bones & Joints
    if (skeletonDraw_) {
        const int J = static_cast<int>(pose_.size());
        const Color colBone = Color{75, 163, 227, 230};
        const Color colJoint = Color{56, 168, 232, 255};
        const Color colRoot = Color{245, 215, 45, 255};

        if (J > 0 && poseParents_.size() == pose_.size()) {
            for (int j = 0; j < J; ++j) {
                const int p = poseParents_[j];
                if (p >= 0 && p < J) {
                    DrawCapsule(pose_[p], pose_[j], 0.028f, 8, 8, colBone);
                }
            }
            for (int j = 0; j < J; ++j) {
                DrawSphere(pose_[j], j == 0 ? 0.055f : 0.032f,
                           j == 0 ? colRoot : colJoint);
            }
        } else if (!characterDraw_ || !character_ || !character_->isLoaded()) {
            // Standby origin rig stub
            DrawSphere(Vector3{0, 1.0f, 0}, 0.065f, colRoot);
            DrawSphere(Vector3{0, 1.6f, 0}, 0.045f, colJoint);
            DrawCapsule(Vector3{0, 0.4f, 0}, Vector3{0, 1.6f, 0}, 0.028f, 8, 8, colBone);
        }
    }

    // 3. Draw Bone Names if enabled
    if (boneNamesDraw_) {
        drawBoneNames(camera_);
    }

    EndMode3D();
}

} // namespace studio
