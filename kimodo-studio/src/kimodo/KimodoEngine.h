#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "kimodo/KimodoAdapter.h"

namespace studio {

enum class EEngineStatus {
    Idle,
    LoadingModel,
    Generating,
    Finished,
    Error,
};

// Owns adapter + single worker thread. UI polls status()/message()
// each frame; worker never touches ImGui or Raylib.
class FKimodoEngine {
public:
    FKimodoEngine() = default;
    ~FKimodoEngine() { Shutdown(); }

    FKimodoEngine(const FKimodoEngine&) = delete;
    FKimodoEngine& operator=(const FKimodoEngine&) = delete;

    void SetPaths(std::string motionGguf, std::string textBundle);

    // Starts async load (if needed) + generate. No-op while busy.
    void requestGenerate(std::string prompt, FGenerationParams params);
    void cancel();

    EEngineStatus GetStatus() const { return status.load(); }
    std::string GetMessage() const;
    std::string GetLastPrompt() const;
    bool IsBusy() const;

    // Real sampler progress, written by worker callback, read by UI.
    float GetProgress() const;
    unsigned GetStepsDone() const { return stepsDone.load(); }
    unsigned GetStepsTotal() const { return stepsTotal.load(); }
    bool sampling() const { return bSampling.load(); }

    // Unloads model so new paths take effect. No-op while busy.
    void unloadModel();

    // Last successful result (copied under lock).
    bool lastResult(FMotionResult& out) const;

    void Shutdown();

private:
    void Run(std::string prompt, FGenerationParams params);

    FKimodoAdapter Adapter;
    std::string MotionPath;
    std::string TextBundle;
    std::thread worker;
    std::atomic<EEngineStatus> status{EEngineStatus::Idle};
    std::atomic<unsigned> stepsDone{0};
    std::atomic<unsigned> stepsTotal{0};
    std::atomic<bool> bSampling{false};
    std::atomic<bool> bCancelRequested{false};
    mutable std::mutex mutex;
    std::string Message = "idle";
    std::string LastPrompt;
    FMotionResult result;
    bool bHasResult = false;
};

} // namespace studio
