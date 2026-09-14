#include "animation/AnimationPlayer.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace studio {

void AnimationPlayer::load(const Animation& anim) {
    anim_ = anim;
    time_ = 0.0f;
    playing_ = true;
    sample();
}

void AnimationPlayer::clear() {
    anim_ = Animation{};
    time_ = 0.0f;
    playing_ = false;
    world_.clear();
}

void AnimationPlayer::update(float dt) {
    if (!hasAnimation() || !playing_) {
        return;
    }
    time_ += dt;
    const float dur = anim_.duration();
    if (time_ >= dur) {
        if (loop_ && dur > 0.0f) {
            time_ = std::fmod(time_, dur);
        } else {
            time_ = dur;
            playing_ = false;
        }
    }
    sample();
}

void AnimationPlayer::scrub(float timeSec) {
    if (!hasAnimation()) {
        return;
    }
    time_ = std::clamp(timeSec, 0.0f, anim_.duration());
    sample();
}

int AnimationPlayer::frame() const {
    if (!hasAnimation()) {
        return 0;
    }
    int f = static_cast<int>(time_ * anim_.fps);
    return std::clamp(f, 0, anim_.frames - 1);
}

void AnimationPlayer::sample() {
    if (!hasAnimation() || anim_.joints != kSomaJoints) {
        world_.clear();
        return;
    }
    const float f = std::clamp(time_ * anim_.fps, 0.0f,
                               static_cast<float>(anim_.frames - 1));
    const int i0 = static_cast<int>(f);
    const int i1 = std::min(i0 + 1, anim_.frames - 1);
    const float a = f - static_cast<float>(i0);

    const int J = anim_.joints;
    const float* r0 = anim_.localRotationsXyzw.data() + static_cast<size_t>(i0) * J * 4;
    const float* r1 = anim_.localRotationsXyzw.data() + static_cast<size_t>(i1) * J * 4;
    const float* p0 = anim_.rootPositions.data() + static_cast<size_t>(i0) * 3;
    const float* p1 = anim_.rootPositions.data() + static_cast<size_t>(i1) * 3;

    std::vector<float> quats(static_cast<size_t>(J) * 4);
    for (int j = 0; j < J; ++j) {
        Quaternion q0{r0[j * 4], r0[j * 4 + 1], r0[j * 4 + 2], r0[j * 4 + 3]};
        Quaternion q1{r1[j * 4], r1[j * 4 + 1], r1[j * 4 + 2], r1[j * 4 + 3]};
        Quaternion q = QuaternionSlerp(q0, q1, a);
        quats[j * 4] = q.x;
        quats[j * 4 + 1] = q.y;
        quats[j * 4 + 2] = q.z;
        quats[j * 4 + 3] = q.w;
    }
    float root[3] = {
        p0[0] + (p1[0] - p0[0]) * a,
        p0[1] + (p1[1] - p0[1]) * a,
        p0[2] + (p1[2] - p0[2]) * a,
    };
    Skeleton::forwardKinematics(quats.data(), root, world_);
}

} // namespace studio
