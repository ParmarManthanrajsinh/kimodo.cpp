#include "kimodo/KimodoEngine.h"

#include "utils/Logger.h"

namespace studio {

void FKimodoEngine::SetPaths(std::string inMotionGguf, std::string inTextBundle) {
    std::lock_guard<std::mutex> lock(mutex);
    MotionPath = std::move(inMotionGguf);
    TextBundle = std::move(inTextBundle);
}

void FKimodoEngine::requestGenerate(std::string prompt, FGenerationParams params) {
    if (IsBusy()) {
        return;
    }
    if (worker.joinable()) {
        worker.join();
    }
    status.store(EEngineStatus::Generating);
    stepsDone.store(0);
    stepsTotal.store(params.steps);
    bSampling.store(false);
    bCancelRequested.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex);
        Message = "starting worker";
        LastPrompt = prompt;
    }
    worker = std::thread(&FKimodoEngine::Run, this, std::move(prompt), params);
}

void FKimodoEngine::cancel() {
    bCancelRequested.store(true);
}

float FKimodoEngine::GetProgress() const {
    const unsigned total = stepsTotal.load();
    if (total == 0) {
        return 0.0f;
    }
    float p = static_cast<float>(stepsDone.load()) / static_cast<float>(total);
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

std::string FKimodoEngine::GetMessage() const {
    std::lock_guard<std::mutex> lock(mutex);
    return Message;
}

std::string FKimodoEngine::GetLastPrompt() const {
    std::lock_guard<std::mutex> lock(mutex);
    return LastPrompt;
}

void FKimodoEngine::unloadModel() {
    if (IsBusy()) {
        return;
    }
    if (worker.joinable()) {
        worker.join();
    }
    Adapter.unload();
    std::lock_guard<std::mutex> lock(mutex);
    Message = "model unloaded (paths apply on next generate)";
    status.store(EEngineStatus::Idle);
}

bool FKimodoEngine::IsBusy() const {
    EEngineStatus s = status.load();
    return s == EEngineStatus::LoadingModel || s == EEngineStatus::Generating;
}

bool FKimodoEngine::lastResult(FMotionResult& out) const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!bHasResult) {
        return false;
    }
    out = result;
    return true;
}

void FKimodoEngine::Shutdown() {
    if (worker.joinable()) {
        worker.join();
    }
    Adapter.unload();
}

void FKimodoEngine::Run(std::string prompt, FGenerationParams params) {
    std::string localMotionPath;
    std::string localTextBundle;
    {
        std::lock_guard<std::mutex> lock(mutex);
        localMotionPath = MotionPath;
        localTextBundle = TextBundle;
    }
    if (!Adapter.IsLoaded()) {
        if (localMotionPath.empty()) {
            std::lock_guard<std::mutex> lock(mutex);
            Message = "No motion model installed. Please open Models page to download or import SOMA weights.";
            status.store(EEngineStatus::Error);
            FLogger::GetInstance().error("Kimodo load aborted: No motion model installed (motion_gguf path is empty). Open Models page.");
            return;
        }

        status.store(EEngineStatus::LoadingModel);
        {
            std::lock_guard<std::mutex> lock(mutex);
            Message = "Loading model...";
        }
        FLogger::GetInstance().info("Kimodo: loading model from " + localMotionPath);
        std::string error;
        if (!Adapter.load(localMotionPath, localTextBundle, error)) {
            std::lock_guard<std::mutex> lock(mutex);
            Message = "Model load failed: " + error;
            status.store(EEngineStatus::Error);
            FLogger::GetInstance().error("Kimodo load failed: " + error);
            return;
        }
        FLogger::GetInstance().info("Kimodo: model loaded successfully");
    }

    status.store(EEngineStatus::Generating);
    {
        std::lock_guard<std::mutex> lock(mutex);
        Message = "Preparing (weights / encoding)...";
    }
    FLogger::GetInstance().info("Kimodo: generation started: " + prompt);
    FMotionResult localResult;
    std::string error;
    auto onProgress = [this](unsigned done, unsigned total) {
        stepsDone.store(done);
        stepsTotal.store(total);
        bSampling.store(true);
        return bCancelRequested.load();
    };
    if (!Adapter.generate(prompt, params, result, error, onProgress)) {
        std::lock_guard<std::mutex> lock(mutex);
        if (error == "generation cancelled" || bCancelRequested.load()) {
            Message = "Generation cancelled";
            status.store(EEngineStatus::Idle);
            FLogger::GetInstance().info("Kimodo: generation cancelled by user");
        } else {
            Message = "Generation failed: " + error;
            status.store(EEngineStatus::Error);
            FLogger::GetInstance().error("Kimodo generate failed: " + error);
        }
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        result = std::move(localResult);
        bHasResult = true;
        Message = "Finished: " + std::to_string(localResult.frames) + " frames, " +
                   std::to_string(localResult.joints) + " joints";
    }
    status.store(EEngineStatus::Finished);
    FLogger::GetInstance().info("Kimodo: generation finished");
}

} // namespace studio
