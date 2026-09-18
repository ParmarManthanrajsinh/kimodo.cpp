#include "animation/Animation.h"

#include "animation/Skeleton.h"

namespace studio {

void FAnimation::fromMotionResult(const FMotionResult& m, float fpsValue) {
    frames = m.frames;
    joints = m.joints;
    fps = fpsValue;
    skeletonName = "soma30";
    jointNames.assign(FSoma30Spec::names.begin(), FSoma30Spec::names.end());
    parents.assign(FSoma30Spec::parents.begin(), FSoma30Spec::parents.end());
    offsets.assign(FSoma30Spec::offsets.begin(), FSoma30Spec::offsets.end());
    localRotationsXyzw = m.localRotationsXyzw;
    rootPositions = m.rootPositions;
}

} // namespace studio
