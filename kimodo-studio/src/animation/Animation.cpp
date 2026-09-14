#include "animation/Animation.h"

#include "animation/Skeleton.h"

namespace studio {

void Animation::fromMotionResult(const MotionResult& m, float fpsValue) {
    frames = m.frames;
    joints = m.joints;
    fps = fpsValue;
    skeletonName = "soma30";
    jointNames.assign(Soma30Spec::names.begin(), Soma30Spec::names.end());
    parents.assign(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
    offsets.assign(Soma30Spec::offsets.begin(), Soma30Spec::offsets.end());
    localRotationsXyzw = m.localRotationsXyzw;
    rootPositions = m.rootPositions;
}

} // namespace studio
