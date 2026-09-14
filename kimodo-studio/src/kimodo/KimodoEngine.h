#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "kimodo/KimodoAdapter.h"

namespace studio {

enum class EngineStatus {
    Idle,
    LoadingModel,
    Generating,
    Finished,
    Error,
};

// Owns adapter + single worker thread. UI polls status()/message()
// each frame; worker never touches ImGui or Raylib.
class KimodoEngine {
public:
    KimodoEngine() = default;
    ~KimodoEngine() { shutdown(); }

    KimodoEngine(const KimodoEngine&) = delete;
    KimodoEngine& operator=(const KimodoEngine&) = delete;

    void setPaths(std::string motionGguf, std::string textBundle);

    // Starts async load (if needed) + generate. No-op while busy.
    void requestGenerate(std::string prompt, GenerationParams params);
    void cancel();

    EngineStatus status() const { return status_.load(); }
    std::string message() const;
    std::string lastPrompt() const;
    bool busy() const;

    // Real sampler progress, written by worker callback, read by UI.
    float progress() const;
    unsigned stepsDone() const { return stepsDone_.load(); }
    unsigned stepsTotal() const { return stepsTotal_.load(); }
    bool sampling() const { return sampling_.load(); }

    // Unloads model so new paths take effect. No-op while busy.
    void unloadModel();

    // Last successful result (copied under lock).
    bool lastResult(MotionResult& out) const;

    void shutdown();

private:
    void run(std::string prompt, GenerationParams params);

    KimodoAdapter adapter_;
    std::string motionPath_;
    std::string textBundle_;
    std::thread worker_;
    std::atomic<EngineStatus> status_{EngineStatus::Idle};
    std::atomic<unsigned> stepsDone_{0};
    std::atomic<unsigned> stepsTotal_{0};
    std::atomic<bool> sampling_{false};
    std::atomic<bool> cancelRequested_{false};
    mutable std::mutex mutex_;
    std::string message_ = "idle";
    std::string lastPrompt_;
    MotionResult result_;
    bool hasResult_ = false;
};

} // namespace studio
