#include "rendering/Viewport.h"

#include <cmath>

#include "animation/Skeleton.h"
#include "raylib.h"
#include "raymath.h"

namespace studio {

void Viewport::reset() {
    target_ = {0, 1, 0};
    yaw_ = 0.7f;
    pitch_ = 0.45f;
    dist_ = 8.0f;
    recomputeCamera();
}

void Viewport::frame() {
    target_ = {0, 1, 0};
    recomputeCamera();
}

void Viewport::update(bool mouseOverUi) {
    const Vector2 delta = GetMouseDelta();

    // Camera owns the pointer only when ImGui does not capture it.
    if (!mouseOverUi) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            yaw_ -= delta.x * 0.005f;
            pitch_ -= delta.y * 0.005f;
            if (pitch_ > 1.45f) pitch_ = 1.45f;
            if (pitch_ < -1.45f) pitch_ = -1.45f;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
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
        if (dist_ < 2.0f) dist_ = 2.0f;
        if (dist_ > 60.0f) dist_ = 60.0f;
    }

    if (IsKeyPressed(KEY_R)) {
        reset();
    }
    if (IsKeyPressed(KEY_F)) {
        frame();
    }
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

void Viewport::draw3D() const {
    BeginMode3D(camera_);
    grid_.draw();
    const int J = static_cast<int>(pose_.size());
    if (J > 0 && poseParents_.size() == pose_.size()) {
        for (int j = 0; j < J; ++j) {
            const int p = poseParents_[j];
            if (p >= 0 && p < J) {
                DrawCapsule(pose_[p], pose_[j], 0.035f, 6, 6,
                            Color{120, 170, 255, 255});
            }
        }
        for (int j = 0; j < J; ++j) {
            DrawSphere(pose_[j], j == 0 ? 0.09f : 0.055f,
                       j == 0 ? YELLOW : SKYBLUE);
        }
    } else {
        // No animation yet: origin rig stub.
        DrawSphere(Vector3{0, 1, 0}, 0.12f, SKYBLUE);
        DrawCapsule(Vector3{0, 0.4f, 0}, Vector3{0, 1.6f, 0}, 0.08f, 8, 8,
                    Color{120, 170, 255, 255});
    }
    EndMode3D();
}

} // namespace studio
