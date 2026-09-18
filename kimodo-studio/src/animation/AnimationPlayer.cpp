#include "animation/AnimationPlayer.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace studio {

void FAnimationPlayer::load(const FAnimation& inClip) {
    clip = inClip;
    time = 0.0f;
    bPlaying = true;
    sample();
}

void FAnimationPlayer::clear() {
    clip = FAnimation{};
    time = 0.0f;
    bPlaying = false;
    world.clear();
}

void FAnimationPlayer::Update(float dt) {
    if (!HasAnimation() || !bPlaying) {
        return;
    }
    time += dt;
    const float dur = clip.GetDuration();
    if (time >= dur) {
        if (bLoop && dur > 0.0f) {
            time = std::fmod(time, dur);
        } else {
            time = dur;
            bPlaying = false;
        }
    }
    sample();
}

void FAnimationPlayer::scrub(float timeSec) {
    if (!HasAnimation()) {
        return;
    }
    time = std::clamp(timeSec, 0.0f, clip.GetDuration());
    sample();
}

void FAnimationPlayer::stepFrame(int delta) {
    if (!HasAnimation() || clip.fps <= 0.0f) {
        return;
    }
    float dt = static_cast<float>(delta) / clip.fps;
    scrub(time + dt);
}

int FAnimationPlayer::Frame() const {
    if (!HasAnimation()) {
        return 0;
    }
    int f = static_cast<int>(time * clip.fps);
    return std::clamp(f, 0, clip.frames - 1);
}

void FAnimationPlayer::sample() {
    if (!HasAnimation() ||
        static_cast<int>(clip.parents.size()) != clip.joints ||
        static_cast<int>(clip.offsets.size()) != clip.joints) {
        world.clear();
        return;
    }
    const float f = std::clamp(time * clip.fps, 0.0f,
                               static_cast<float>(clip.frames - 1));
    const int i0 = static_cast<int>(f);
    const int i1 = std::min(i0 + 1, clip.frames - 1);
    const float a = f - static_cast<float>(i0);

    const int J = clip.joints;
    const float* r0 = clip.localRotationsXyzw.data() + static_cast<size_t>(i0) * J * 4;
    const float* r1 = clip.localRotationsXyzw.data() + static_cast<size_t>(i1) * J * 4;
    const float* p0 = clip.rootPositions.data() + static_cast<size_t>(i0) * 3;
    const float* p1 = clip.rootPositions.data() + static_cast<size_t>(i1) * 3;

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
    FSkeleton::ForwardKinematicsGeneral(quats.data(), root, clip.parents,
                                        clip.offsets, world);
}

} // namespace studio
