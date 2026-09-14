#include "animation/Animation.h"

namespace studio {

void Animation::fromMotionResult(const MotionResult& m, float fpsValue) {
    frames = m.frames;
    joints = m.joints;
    fps = fpsValue;
    localRotationsXyzw = m.localRotationsXyzw;
    rootPositions = m.rootPositions;
}

} // namespace studio
