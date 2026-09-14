#pragma once

#include <vector>

#include "animation/Animation.h"
#include "animation/Skeleton.h"
#include "raylib.h"

namespace studio {

// Playback state machine. UI thread only. Samples interpolated pose
// each frame and runs forward kinematics into world positions.
class AnimationPlayer {
public:
    void load(const Animation& anim);
    void clear();

    void play() { playing_ = true; }
    void pause() { playing_ = false; }
    void toggle() { playing_ = !playing_; }
    void restart() { time_ = 0.0f; }
    void setLoop(bool loop) { loop_ = loop; }

    void update(float dt);
    void scrub(float timeSec);

    bool hasAnimation() const { return !anim_.empty(); }
    bool playing() const { return playing_; }
    bool loop() const { return loop_; }
    float time() const { return time_; }
    float duration() const { return anim_.duration(); }
    float fps() const { return anim_.fps; }
    int frame() const;
    const std::vector<Vector3>& worldPositions() const { return world_; }

private:
    void sample();

    Animation anim_;
    float time_ = 0.0f;
    bool playing_ = false;
    bool loop_ = true;
    std::vector<Vector3> world_;
};

} // namespace studio
