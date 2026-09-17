#pragma once

#include <algorithm>
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
    void togglePlay() { toggle(); }
    void restart() { time_ = 0.0f; }
    void setLoop(bool loop) { loop_ = loop; }

    void update(float dt);
    void scrub(float timeSec);
    void stepFrame(int delta);
    void seekFrame(int f) {
        if (anim_.fps > 0.0f) scrub(static_cast<float>(f) / anim_.fps);
    }

    bool hasAnimation() const { return !anim_.empty(); }
    const Animation& animation() const { return anim_; }
    const std::vector<int>& poseParents() const { return anim_.parents; }
    bool playing() const { return playing_; }
    bool isPlaying() const { return playing_; }
    bool loop() const { return loop_; }
    bool isLooping() const { return loop_; }
    float time() const { return time_; }
    float duration() const { return anim_.duration(); }
    float fps() const { return anim_.fps; }
    int frame() const;
    int totalFrames() const { return anim_.frames; }
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
