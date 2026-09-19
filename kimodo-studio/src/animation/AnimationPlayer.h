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
    void Load(const Animation& in_clip);
    void Clear();

    void Play() { playing = true; }
    void Pause() { playing = false; }
    void Toggle() { playing = !playing; }
    void TogglePlay() { Toggle(); }
    void Restart() { time = 0.0f; }
    void SetLoop(bool enable) { loop = enable; }

    void Update(float dt);
    void Scrub(float time_sec);
    void StepFrame(int delta);
    void SeekFrame(int f) {
        if (clip.fps > 0.0f)
            Scrub(static_cast<float>(f) / clip.fps);
    }

    bool HasAnimation() const { return !clip.empty(); }
    const Animation& GetAnimation() const { return clip; }
    const std::vector<int>& GetPoseParents() const { return clip.parents; }
    bool IsPlaying() const { return playing; }
    bool IsLooping() const { return loop; }
    float GetTime() const { return time; }
    float GetDuration() const { return clip.GetDuration(); }
    float GetFps() const { return clip.fps; }
    int Frame() const;
    int GetTotalFrames() const { return clip.frames; }
    const std::vector<Vector3>& GetWorldPositions() const { return world; }

private:
    void Sample();

    Animation clip;
    float time = 0.0f;
    bool playing = false;
    bool loop = true;
    std::vector<Vector3> world;
};

} // namespace studio
