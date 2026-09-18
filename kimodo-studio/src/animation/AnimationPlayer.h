#pragma once

#include <algorithm>
#include <vector>

#include "animation/Animation.h"
#include "animation/Skeleton.h"
#include "raylib.h"

namespace studio {

// Playback state machine. UI thread only. Samples interpolated pose
// each frame and runs forward kinematics into world positions.
class FAnimationPlayer {
public:
    void load(const FAnimation& inClip);
    void clear();

    void play() { bPlaying = true; }
    void pause() { bPlaying = false; }
    void toggle() { bPlaying = !bPlaying; }
    void togglePlay() { toggle(); }
    void restart() { time = 0.0f; }
    void SetLoop(bool loop) { bLoop = loop; }

    void Update(float dt);
    void scrub(float timeSec);
    void stepFrame(int delta);
    void seekFrame(int f) {
        if (clip.fps > 0.0f) scrub(static_cast<float>(f) / clip.fps);
    }

    bool HasAnimation() const { return !clip.empty(); }
    const FAnimation& GetAnimation() const { return clip; }
    const std::vector<int>& GetPoseParents() const { return clip.parents; }
    bool IsPlaying() const { return bPlaying; }
    bool IsLooping() const { return bLoop; }
    float GetTime() const { return time; }
    float GetDuration() const { return clip.GetDuration(); }
    float GetFps() const { return clip.fps; }
    int Frame() const;
    int GetTotalFrames() const { return clip.frames; }
    const std::vector<Vector3>& GetWorldPositions() const { return world; }

private:
    void sample();

    FAnimation clip;
    float time = 0.0f;
    bool bPlaying = false;
    bool bLoop = true;
    std::vector<Vector3> world;
};

} // namespace studio
