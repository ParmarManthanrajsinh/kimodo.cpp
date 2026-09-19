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
    ~KimodoEngine() { Shutdown(); }

    KimodoEngine(const KimodoEngine&) = delete;
    KimodoEngine& operator=(const KimodoEngine&) = delete;

    void SetPaths(std::string motion_gguf, std::string text_bundle);

    // Starts async load (if needed) + generate. No-op while busy.
    void RequestGenerate(std::string prompt, GenerationParams params);
    void Cancel();

    EngineStatus GetStatus() const { return status.load(); }
    std::string GetMessage() const;
    std::string GetLastPrompt() const;
    bool IsBusy() const;

    // Real sampler progress, written by worker callback, read by UI.
    float GetProgress() const;
    unsigned GetStepsDone() const { return steps_done.load(); }
    unsigned GetStepsTotal() const { return steps_total.load(); }
    bool Sampling() const { return sampling.load(); }

    // Unloads model so new paths take effect. No-op while busy.
    void UnloadModel();

    // Last successful result (copied under lock).
    bool LastResult(MotionResult& out) const;

    void Shutdown();

private:
    void Run(std::string prompt, GenerationParams params);

    KimodoAdapter adapter;
    std::string motion_path;
    std::string text_bundle;
    std::thread worker;
    std::atomic<EngineStatus> status{EngineStatus::Idle};
    std::atomic<unsigned> steps_done{0};
    std::atomic<unsigned> steps_total{0};
    std::atomic<bool> sampling{false};
    std::atomic<bool> cancel_requested{false};
    mutable std::mutex mutex;
    std::string message = "idle";
    std::string last_prompt;
    MotionResult result;
    bool has_result = false;
};

} // namespace studio
