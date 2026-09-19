#include "animation/Animation.h"

#include "animation/Skeleton.h"

namespace studio
{

void Animation::FromMotionResult(const MotionResult& m, float fps_value)
{
    frames = m.frames;
    joints = m.joints;
    fps = fps_value;
    skeleton_name = "soma30";
    joint_names.assign(Soma30Spec::names.begin(), Soma30Spec::names.end());
    parents.assign(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
    offsets.assign(Soma30Spec::offsets.begin(), Soma30Spec::offsets.end());
    local_rotations_xyzw = m.local_rotations_xyzw;
    root_positions = m.root_positions;
}

} // namespace studio
